#ifndef _REPOSITORY_V8_H_
#define _REPOSITORY_V8_H_

v8::Handle<v8::Value> repo_create(void*);
void repo_remove(const v8::FunctionCallbackInfo<v8::Value>&);
void repo_commit(const v8::FunctionCallbackInfo<v8::Value>&);
void repo_fetch(const v8::FunctionCallbackInfo<v8::Value>&);
void repo_merge(const v8::FunctionCallbackInfo<v8::Value>&);
void repo_push(const v8::FunctionCallbackInfo<v8::Value>&);

#endif // _REPOSITORY_V8_H_
