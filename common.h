#ifndef _GITDIST_COMMON_H_
#define _GITDIST_COMMON_H_

v8::Handle<v8::Object> Error(int id);
v8::Handle<v8::Function> wrap_func(v8::FunctionCallback, const char *name=NULL);
int check_args(const v8::FunctionCallbackInfo<v8::Value>&, v8::Handle<v8::Value>*);

#endif // _GITDIST_COMMON_H_
