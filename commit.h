#ifndef _COMMIT_GIT_H_
#define _COMMIT_GIT_H_

v8::Handle<v8::Object> commit_create(git_commit*);
void commit_remove(const v8::FunctionCallbackInfo<v8::Value>&);

#endif // _COMMIT_GIT_H_
