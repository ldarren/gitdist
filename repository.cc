#include <node.h>
#include <git2.h>
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
printf("born\n");
        git_oid tree_id, parent_id, commit_id;
        git_tree *tree;
        git_commit *parent;
        git_index *index;

        /* Get the index and write it to a tree */
        rc = git_repository_index(&index, repo);
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
    }else{ // initial commit
printf("unborn\n");
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

void repo_pull(const v8::FunctionCallbackInfo<v8::Value> &args){
    printf("pull\n");
}

void repo_push(const v8::FunctionCallbackInfo<v8::Value> &args){
    printf("push\n");
}
