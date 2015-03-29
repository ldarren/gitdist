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
    git_oid oid_blob;    /* the SHA1 for our blob in the tree */
    git_oid oid_tree;    /* the SHA1 for our tree in the commit */
    git_oid oid_commit;  /* the SHA1 for our initial commit */
    git_blob * blob;     /* our blob in the tree */
    git_tree * tree_cmt; /* our tree in the commit */
    git_treebuilder * tree_bld;  /* tree builder */

    /* create a blob from our buffer */
    rc = git_blob_create_fromworkdir( &oid_blob, repo, *path);
    if (rc) return next(iso, cb, rc);
    rc = git_blob_lookup( &blob, repo, &oid_blob );
    if (rc) return next(iso, cb, rc);
    rc = git_treebuilder_create( &tree_bld, NULL );
    if (rc) return next(iso, cb, rc);
    rc = git_treebuilder_insert( NULL, tree_bld, "name-of-the-file.txt", &oid_blob, GIT_FILEMODE_BLOB );
    if (rc) return next(iso, cb, rc);
    rc = git_treebuilder_write( &oid_tree, repo, tree_bld );
    if (rc) return next(iso, cb, rc);
    rc = git_tree_lookup( &tree_cmt, repo, &oid_tree );
    if (rc) return next(iso, cb, rc);
    rc = git_signature_now(&sign, "ldarren", "ldarren@gmail.com");
    if (rc) return next(iso, cb, rc);
    rc = git_commit_create(
        &oid_commit, repo, "HEAD",
        sign, sign, /* same author and commiter */
        NULL, /* default UTF-8 encoding */
        message,
        tree_cmt, 0, NULL );

    git_signature_free(sign);
    git_tree_free( tree_cmt );
    git_treebuilder_free( tree_bld );
    git_blob_free( blob );

    git_libgit2_shutdown();

    return next(iso, cb, rc);

}

void repo_pull(const v8::FunctionCallbackInfo<v8::Value> &args){
    printf("pull\n");
}

void repo_push(const v8::FunctionCallbackInfo<v8::Value> &args){
    printf("push\n");
}
