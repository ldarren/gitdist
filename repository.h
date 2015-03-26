#ifndef _REPOSITORY_V8_H_
#define _REPOSITORY_V8_H_

v8::Handle<v8::Value> repo_create(void*);
void repo_remove(const v8::FunctionCallbackInfo<v8::Value>&);
void repo_commit_get(const v8::FunctionCallbackInfo<v8::Value>&);
void repo_pull(const v8::FunctionCallbackInfo<v8::Value>&);
void repo_push(const v8::FunctionCallbackInfo<v8::Value>&);

#endif // _REPOSITORY_V8_H_
