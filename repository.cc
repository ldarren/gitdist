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
    self->Set(String::NewFromUtf8(iso, "pull"), wrap_func(repo_pull));
    self->Set(String::NewFromUtf8(iso, "commit"), wrap_func(repo_commit_get));
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

void repo_commit_get(const v8::FunctionCallbackInfo<v8::Value> &args){
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
    if (rc) return next(iso, cb, rc);

    if (/*git_repository_head_unborn(repo)*/1){
        git_oid tree_id, parent_id, commit_id;
        git_index *index;
        git_tree *tree;
        git_commit *parent;

        /* Get the index and write it to a tree */
        rc = git_repository_index(&index, repo);
        if (rc) return next(iso, cb, rc);
        rc = git_index_read(index, 1);
        if (rc) return next(iso, cb, rc);
        rc = git_index_add_bypath(index, *path);
        if (rc) return next(iso, cb, rc);
        rc = git_index_write(index);
        if (rc) return next(iso, cb, rc);
        rc = git_index_write_tree(&tree_id, index);
        if (rc) return next(iso, cb, rc);
        rc = git_tree_lookup(&tree, repo, &tree_id);
        if (rc) return next(iso, cb, rc);

        /* Get HEAD as a commit object to use as the parent of the commit */
        rc = git_reference_name_to_id(&parent_id, repo, "HEAD");
        if (rc) return next(iso, cb, rc);
        rc = git_commit_lookup(&parent, repo, &parent_id);
        if (rc) return next(iso, cb, rc);

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
        if (rc) return next(iso, cb, rc);
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
        if (rc) return next(iso, cb, rc);
        rc = git_blob_lookup( &blob, repo, &oid_blob );
        if (rc) return next(iso, cb, rc);
        rc = git_treebuilder_new( &tree_bld, repo, NULL );
        if (rc) return next(iso, cb, rc);
        rc = git_treebuilder_insert( NULL, tree_bld, "name-of-the-file.txt", &oid_blob, GIT_FILEMODE_BLOB );
        if (rc) return next(iso, cb, rc);
        rc = git_treebuilder_write( &oid_tree, tree_bld );
        if (rc) return next(iso, cb, rc);
        rc = git_tree_lookup( &tree_cmt, repo, &oid_tree );
        if (rc) return next(iso, cb, rc);
        rc = git_commit_create(
            &oid_commit, repo, "HEAD",
            sign, sign, /* same author and commiter */
            NULL, /* default UTF-8 encoding */
            *comment,
            tree_cmt, 0, NULL );

        git_tree_free( tree_cmt );
        git_treebuilder_free( tree_bld );
        git_blob_free( blob );
    }

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

void repo_pull(const v8::FunctionCallbackInfo<v8::Value> &args){
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
    do {
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

    git_signature *sign;
    rc = git_signature_now(&sign, "ldarren", "ldarren@gmail.com");
    if (rc) goto cleanup;
    rc = git_remote_update_tips(remote, sign, NULL);
    if (rc) goto cleanup;

//https://github.com/libgit2/libgit2/issues/1555
/*
//https://github.com/nodegit/nodegit/issues/151
A git pull is a fetch and merge

- Connection to the remote
- Download the pack, I'm guessing this is effectively a fetch,
- then create a diff between the current head and the fetched content.
- Do a merge if needed on the diff and create a patch, then apply the patch externally or with some other module.
- Finally create another commit to represent this merge.
- If a fast-forward is possible (Nothing needs to be merged because the commit your local branch is on is a direct ancestor of the commit you're pulling), then you can perhaps just repoint the index.
 */
/*
repo.fetchAll({
    credentials: function(url, userName) {
        return NodeGit.Cred.sshKeyFromAgent(userName);
    }
}).then(function() {
    repo.mergeBranches("master", "origin/master");
});
 */
//git_merge_trees
//git_index_has_conflicts //https://github.com/libgit2/libgit2/issues/1567
//git_index_write_tree
//git_commit_create

cleanup:
    git_remote_free(remote);
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
