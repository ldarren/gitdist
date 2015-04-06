#include <node.h>
#include <git2.h>
#include <unistd.h>
#include "./object_v8.h"
#include "./common.h"
#include "./repository.h"
#include "./commit.h"

using namespace v8;

Handle<Value> repo_create(void *ptr){
    Isolate* iso = Isolate::GetCurrent();
    EscapableHandleScope scope(iso);

    Local<Object> self = self_alloc(ptr);
    self->Set(String::NewFromUtf8(iso, "fetch"), wrap_func(repo_fetch));
    self->Set(String::NewFromUtf8(iso, "merge"), wrap_func(repo_merge));
    self->Set(String::NewFromUtf8(iso, "commit"), wrap_func(repo_commit));
    self->Set(String::NewFromUtf8(iso, "push"), wrap_func(repo_push));
    self->Set(String::NewFromUtf8(iso, "free"), wrap_func(repo_remove));

    return scope.Escape(self);
}

void repo_remove(const FunctionCallbackInfo<Value>& args){
    Isolate* iso = Isolate::GetCurrent();
    HandleScope scope(iso);

    git_repository *repo = (git_repository*)self_get(args);    
    git_libgit2_init();
    git_repository_free(repo);
    self_free(args);
    git_libgit2_shutdown();

    args.GetReturnValue().Set(Number::New(iso, 0));
}

void repo_commit(const v8::FunctionCallbackInfo<v8::Value> &args){
    Isolate *iso = Isolate::GetCurrent();
    HandleScope scope(iso);

    Local<String> aPath;
    Local<String> aComment;
    Local<Function> cb;

    switch(args.Length()){
    case 3:
        aPath = args[0]->ToString();
        aComment = args[1]->ToString();
        cb = Local<Function>::Cast(args[2]);
        break;
    default:
        iso->ThrowException(Exception::TypeError(String::NewFromUtf8(iso, "Wrong number of arguments")));
        return;
    }
    String::Utf8Value path(aPath);
    String::Utf8Value comment(aComment);

    git_libgit2_init();

    git_repository *repo = (git_repository*)self_get(args);    
    int rc;              /* return code for git_ functions */
    git_signature *sign;

    rc = git_signature_now(&sign, "ldarren", "ldarren@gmail.com");
    if (rc) goto cleanup;

    if (/*git_repository_head_unborn(repo)*/1){
        git_oid tree_id, parent_id, commit_id;
        git_index *index;
        git_tree *tree;
        git_commit *parent;

        /* Get the index and write it to a tree */
        rc = git_repository_index(&index, repo);
        if (rc) goto cleanup_1;
        rc = git_index_read(index, 1);
        if (rc) goto cleanup_1;
        rc = git_index_add_bypath(index, *path);
        if (rc) goto cleanup_1;
        rc = git_index_write(index);
        if (rc) goto cleanup_1;
        rc = git_index_write_tree(&tree_id, index);
        if (rc) goto cleanup_1;
        rc = git_tree_lookup(&tree, repo, &tree_id);
        if (rc) goto cleanup_1;

        /* Get HEAD as a commit object to use as the parent of the commit */
        rc = git_reference_name_to_id(&parent_id, repo, "HEAD");
        if (rc) goto cleanup_1;
        rc = git_commit_lookup(&parent, repo, &parent_id);
        if (rc) goto cleanup_1;

        /* Do the commit */
        rc = git_commit_create_v(
            &commit_id,
            repo,
            "HEAD",     /* The commit will update the position of HEAD */
            sign,
            sign,
            NULL,       /* UTF-8 encoding */
            *comment,
            tree,       /* The tree from the index */
            1,          /* Only one parent */
            parent      /* No need to make a list with create_v */
        );
        if (rc) goto cleanup_1;

cleanup_1:
        git_commit_free( parent );
        git_tree_free( tree );
        git_index_free( index );
    }else{ // initial commit
        git_oid oid_blob;    /* the SHA1 for our blob in the tree */
        git_oid oid_tree;    /* the SHA1 for our tree in the commit */
        git_oid oid_commit;  /* the SHA1 for our initial commit */
        git_blob * blob;     /* our blob in the tree */
        git_tree * tree_cmt; /* our tree in the commit */
        git_treebuilder * tree_bld;  /* tree builder */

        rc = git_blob_create_fromworkdir( &oid_blob, repo, *path);
        if (rc) goto cleanup_2;
        rc = git_blob_lookup( &blob, repo, &oid_blob );
        if (rc) goto cleanup_2;
        rc = git_treebuilder_new( &tree_bld, repo, NULL );
        if (rc) goto cleanup_2;
        rc = git_treebuilder_insert( NULL, tree_bld, "name-of-the-file.txt", &oid_blob, GIT_FILEMODE_BLOB );
        if (rc) goto cleanup_2;
        rc = git_treebuilder_write( &oid_tree, tree_bld );
        if (rc) goto cleanup_2;
        rc = git_tree_lookup( &tree_cmt, repo, &oid_tree );
        if (rc) goto cleanup_2;
        rc = git_commit_create(
            &oid_commit, repo, "HEAD",
            sign, sign, /* same author and commiter */
            NULL, /* default UTF-8 encoding */
            *comment,
            tree_cmt, 0, NULL );
        if (rc) goto cleanup_2;

cleanup_2:
        git_tree_free( tree_cmt );
        git_treebuilder_free( tree_bld );
        git_blob_free( blob );
    }

cleanup:
    git_signature_free(sign);
    git_libgit2_shutdown();

    return next(iso, cb, rc);

}

struct dl_data {
  git_remote *remote;
  int ret;
  int finished;
};

static int progress_cb(const char *str, int len, void *data) {
  (void)data;
  printf("remote: %.*s", len, str);
  fflush(stdout); /* We don't have the \n to force the flush */
  return 0;
}

static int update_cb(const char *refname, const git_oid *a, const git_oid *b, void *data) {
  char a_str[GIT_OID_HEXSZ+1], b_str[GIT_OID_HEXSZ+1];
  (void)data;

  git_oid_fmt(b_str, b);
  b_str[GIT_OID_HEXSZ] = '\0';

  if (git_oid_iszero(a)) {
    printf("[new]     %.20s %s\n", b_str, refname);
  } else {
    git_oid_fmt(a_str, a);
    a_str[GIT_OID_HEXSZ] = '\0';
    printf("[updated] %.10s..%.10s %s\n", a_str, b_str, refname);
  }

  return 0;
}

static void *download(void *ptr) {
    struct dl_data *data = (struct dl_data *)ptr;

    // Connect to the remote end specifying that we want to fetch
    // information from it.
    if (git_remote_connect(data->remote, GIT_DIRECTION_FETCH) < 0) {
        data->ret = -1;
        goto exit;
    }

    // Download the packfile and index it. This function updates the
    // amount of received data and the indexer stats which lets you
    // inform the user about progress.
    if (git_remote_download(data->remote, NULL) < 0) {
        data->ret = -1;
        goto exit;
    }

    data->ret = 0;

exit:
    data->finished = 1;
    return &data->ret;
}

void repo_fetch(const v8::FunctionCallbackInfo<v8::Value> &args){
    Isolate *iso = Isolate::GetCurrent();
    HandleScope scope(iso);

    Local<String> aName;
    Local<Function> cb;

    switch(args.Length()){
    case 1:
        aName = String::NewFromUtf8(iso, "origin");
        cb = Local<Function>::Cast(args[0]);
        break;
    case 2:
        aName = args[0]->ToString();
        cb = Local<Function>::Cast(args[2]);
        break;
    default:
        iso->ThrowException(Exception::TypeError(String::NewFromUtf8(iso, "Wrong number of arguments")));
        return;
    }
    String::Utf8Value name(aName);

    git_libgit2_init();

    pthread_t worker;
    git_repository *repo = (git_repository*)self_get(args);

    // get the remote.
    int rc;
    const git_transfer_progress *stats;
    struct dl_data data;
    git_remote* remote = NULL;
    rc = git_remote_lookup( &remote, repo, *name );
    if (rc) {
        rc = git_remote_create_anonymous(&remote, repo, *name, NULL);
        if (rc) goto cleanup;
    }

    git_remote_callbacks callbacks;
    git_remote_init_callbacks(&callbacks, GIT_REMOTE_CALLBACKS_VERSION);
    callbacks.update_tips = &update_cb;
    callbacks.sideband_progress = &progress_cb;
    callbacks.credentials = cred_acquire_cb;
    git_remote_set_callbacks(remote, &callbacks);

    data.remote = remote;
    data.ret = 0;
    data.finished = 0;

    stats = git_remote_stats(remote);
    pthread_create(&worker, NULL, download, &data);
    do { // TODO: use v8 event?
        usleep(10000);

        if (stats->received_objects == stats->total_objects) {
            printf("Resolving deltas %d/%d\r",
            stats->indexed_deltas, stats->total_deltas);
        } else if (stats->total_objects > 0) {
            printf("Received %d/%d objects (%d) in %4zu bytes\r",
            stats->received_objects, stats->total_objects,
            stats->indexed_objects, stats->received_bytes);
        }
    } while (!data.finished);

    if (data.ret < 0) goto cleanup;

    pthread_join(worker, NULL);
    if (stats->local_objects > 0) {
        printf("\rReceived %d/%d objects in %zu bytes (used %d local objects)\n",
        stats->indexed_objects, stats->total_objects, stats->received_bytes, stats->local_objects);
    } else{
        printf("\rReceived %d/%d objects in %zu bytes\n",
        stats->indexed_objects, stats->total_objects, stats->received_bytes);
    }

    git_remote_disconnect(remote);

    // TODO repo.defaultSignature
    git_signature *sign;
    rc = git_signature_now(&sign, "ldarren", "ldarren@gmail.com");
    if (rc) goto cleanup;
    rc = git_remote_update_tips(remote, sign, NULL);
    if (rc) goto cleanup;

cleanup:
    git_signature_free(sign);
    git_remote_free(remote);
    git_libgit2_shutdown();

    return next(iso, cb, rc);
}

int get_reference(git_reference **out, git_repository *repo, const char *name){
    int rc;
    git_reference *ref;
    rc = git_reference_dwim(&ref, repo, name);
    if (rc) return rc;
    if (GIT_REF_SYMBOLIC == git_reference_type(ref)){
        rc = git_reference_resolve(out, ref);
        git_reference_free(ref);
        return rc;
    }
    *out = ref;
    return 0;
}

//lib/repository.js
void repo_merge(const v8::FunctionCallbackInfo<v8::Value> &args){
    Isolate *iso = Isolate::GetCurrent();
    HandleScope scope(iso);

    Local<String> aToName;
    Local<String> aFromName;
    Local<Function> cb;

    switch(args.Length()){
    case 1:
        aToName = String::NewFromUtf8(iso, "master");
        aFromName = String::NewFromUtf8(iso, "origin/master");
        cb = Local<Function>::Cast(args[0]);
        break;
    case 3:
        aToName = args[0]->ToString();
        aFromName = args[1]->ToString();
        cb = Local<Function>::Cast(args[2]);
        break;
    default:
        iso->ThrowException(Exception::TypeError(String::NewFromUtf8(iso, "Wrong number of arguments")));
        return;
    }
    String::Utf8Value toName(aToName);
    String::Utf8Value fromName(aFromName);

    git_libgit2_init();
    git_repository *repo = (git_repository*)self_get(args);

    git_reference *toBranch, *fromBranch;
    git_commit *toCommit, *fromCommit;
    int rc;

    //rc = git_branch_lookup(&toBranch, repo, *toName, GIT_BRANCH_LOCAL);
    rc = get_reference(&toBranch, repo, *toName);
    if (rc) goto cleanup;
    //rc = git_branch_lookup(&fromBranch, repo, *fromName, GIT_BRANCH_REMOTE);
    rc = get_reference(&fromBranch, repo, *fromName);
    if (rc) goto cleanup;

    rc = git_commit_lookup(&toCommit, repo, git_reference_target(toBranch));
    if (rc) goto cleanup;
    rc = git_commit_lookup(&fromCommit, repo, git_reference_target(fromBranch));
    if (rc) goto cleanup;
    git_oid baseOid;
    rc = git_merge_base(&baseOid, repo, git_commit_id(toCommit), git_commit_id(fromCommit));
    if (rc) goto cleanup;

    if (0 == git_oid_cmp(&baseOid, git_commit_id(fromCommit))){
        goto cleanup;
    }else if (0 == git_oid_cmp(&baseOid, git_commit_id(toCommit))){
        git_tree *tree = NULL;
        git_signature *sign = NULL;
        git_reference *newFromBranch = NULL;

        rc = git_commit_tree(&tree, fromCommit);
        if (rc) goto cleanup_1;
        if (1 == git_branch_is_head(toBranch)){
            git_checkout_options checkup_opts;
            rc = git_checkout_init_options(&checkup_opts, GIT_CHECKOUT_OPTIONS_VERSION);
            if (rc) goto cleanup_1;
            checkup_opts.checkout_strategy = GIT_CHECKOUT_SAFE;
            rc = git_checkout_tree(repo, (git_object*)tree, &checkup_opts);
            if (rc) goto cleanup_1;
        }

        char buf[64];
        sprintf(buf, "Fast forward branch %s to branch %s", *toName, *fromName);

        rc = git_signature_now(&sign, "ldarren", "ldarren@gmail.com");
        if (rc) goto cleanup_1;

        git_reference_set_target(&newFromBranch, toBranch, git_commit_id(fromCommit), sign, buf);

cleanup_1:
        git_reference_free(newFromBranch);
        git_signature_free(sign);
        git_tree_free(tree);
    }else{
        git_index *index;
        git_tree *tree;
        git_signature *sign;
        const git_commit *parents[] = {toCommit, fromCommit};
        git_oid tree_oid;
        git_oid oid;

        rc = git_merge_commits(&index, repo, toCommit, fromCommit, NULL);
        if (rc) goto cleanup_2;
        if (git_index_has_conflicts(index)){
            iso->ThrowException(Exception::TypeError(String::NewFromUtf8(iso, "Index has conflict")));
        }
        rc = git_index_write(index);
        if (rc) goto cleanup_2;
        rc = git_index_write_tree_to(&tree_oid, index, repo);
        if (rc) goto cleanup_2;
        git_tree_lookup(&tree, repo, &tree_oid);

        char buf[64];
        sprintf(buf, "Merged %s into %s", *fromName, *toName);

        rc = git_signature_now(&sign, "ldarren", "ldarren@gmail.com");
        if (rc) goto cleanup_2;
        rc = git_commit_create(&oid, repo, NULL, sign, sign, NULL, buf, tree, 2, parents);

cleanup_2:
        git_signature_free(sign);
        git_tree_free(tree);
        git_index_free(index);
    }

cleanup:
    git_reference_free(toBranch);
    git_reference_free(fromBranch);
    git_commit_free(toCommit);
    git_commit_free(fromCommit);
    git_libgit2_shutdown();

    return next(iso, cb, rc);
}

void repo_push(const v8::FunctionCallbackInfo<v8::Value> &args){
    Isolate *iso = Isolate::GetCurrent();
    HandleScope scope(iso);

    Local<String> aName;
    Local<String> aRefspec;
    Local<Function> cb;

    switch(args.Length()){
    case 1:
        aName = String::NewFromUtf8(iso, "origin");
        aRefspec = String::NewFromUtf8(iso, "refs/heads/master:refs/heads/master");
        cb = Local<Function>::Cast(args[0]);
        break;
    case 3:
        aName = args[0]->ToString();
        aRefspec = args[1]->ToString();
        cb = Local<Function>::Cast(args[2]);
        break;
    default:
        iso->ThrowException(Exception::TypeError(String::NewFromUtf8(iso, "Wrong number of arguments")));
        return;
    }
    String::Utf8Value name(aName);
    String::Utf8Value refspec(aRefspec);

    git_libgit2_init();

    git_repository *repo = (git_repository*)self_get(args);

    // get the remote.
    int rc;
    git_remote* remote = NULL;
    rc = git_remote_lookup( &remote, repo, *name );
    if (rc) {
        rc = git_remote_create_anonymous(&remote, repo, *name, NULL);
        if (rc) goto cleanup;
    }

    git_remote_callbacks callbacks;
    git_remote_init_callbacks(&callbacks, GIT_REMOTE_CALLBACKS_VERSION);
    callbacks.credentials = cred_acquire_cb;
    git_remote_set_callbacks(remote, &callbacks);

    // connect to remote
    rc = git_remote_connect( remote, GIT_DIRECTION_PUSH );
    if (rc) goto cleanup;

    // add a push refspec
    rc = git_remote_add_push( remote, *refspec );
    if (rc) goto cleanup;

    // configure options
    git_push_options options;
    rc = git_push_init_options( &options, GIT_PUSH_OPTIONS_VERSION );
    if (rc) goto cleanup;

    // do the push
    rc = git_remote_upload( remote, NULL, &options );
    if (rc) goto cleanup;

cleanup:    
    git_remote_disconnect(remote);
    git_remote_free(remote);
    git_libgit2_shutdown();

    return next(iso, cb, rc);
}
