#include <v8.h>
#include <string.h>
#include "./object_v8.h"

using namespace v8;

const char* toUTF8(Handle<String> jsStr, char *buffer){
    String::Utf8Value str(jsStr);
    return strcpy(buffer, *str);
}

ObjectV8::ObjectV8(){
    Isolate *iso = Isolate::GetCurrent();
    _obj.Reset(iso, Object::New(iso));
}
ObjectV8::ObjectV8(Handle<Object> obj){
    Isolate *iso = Isolate::GetCurrent();
    _obj.Reset(iso, obj);
}
ObjectV8::~ObjectV8(){
    _obj.Reset();
}

bool ObjectV8::get(const char *key, bool def) const{
    Isolate *iso = Isolate::GetCurrent();
    HandleScope scope(iso);

    Local<String> p = String::NewFromUtf8(iso,key);
    Local<Object> o = Local<Object>::New(iso,_obj);
    if (o->Has(p)) {
        Local<Value> v = o->Get(p);
        if (v->IsBoolean()){
            return v->BooleanValue();
        }
    }
    return def;
}
int ObjectV8::get(const char *key, int def) const{
    Isolate *iso = Isolate::GetCurrent();
    HandleScope scope(iso);

    Local<String> p = String::NewFromUtf8(iso,key);
    Local<Object> o = Local<Object>::New(iso,_obj);
    if (o->Has(p)) {
        Local<Value> v = o->Get(p);
        if (v->IsInt32()){
            return v->Int32Value();
        }
    }
    return def;
}
unsigned int ObjectV8::get(const char *key, unsigned int def) const{
    Isolate *iso = Isolate::GetCurrent();
    HandleScope scope(iso);

    Local<String> p = String::NewFromUtf8(iso,key);
    Local<Object> o = Local<Object>::New(iso,_obj);
    if (o->Has(p)) {
        Local<Value> v = o->Get(p);
        if (v->IsUint32()){
            return v->Uint32Value();
        }
    }
    return def;
}
double ObjectV8::get(const char *key, double def) const{
    Isolate *iso = Isolate::GetCurrent();
    HandleScope scope(iso);

    Local<String> p = String::NewFromUtf8(iso,key);
    Local<Object> o = Local<Object>::New(iso,_obj);
    if (o->Has(p)) {
        Local<Value> v = o->Get(p);
        if (v->IsNumber()){
            return v->NumberValue();
        }
    }
    return def;
}
/*
 * How to get const char from Handle<String>
 *  Handle<String> testStr = objV8->get("TEST_STR", "NA");
 *  String::Utf8Value utf(testStr);
 *  roster->Set(7, String::New(*uft));
 */
Handle<String> ObjectV8::get(const char *key, const char *def) const{
    Isolate *iso = Isolate::GetCurrent();
    EscapableHandleScope scope(iso); // this will cause core dump
    Local<String> ret;

    Local<String> p = String::NewFromUtf8(iso,key);
    Local<Object> o = Local<Object>::New(iso,_obj);
    if (o->Has(p)) {
        Local<Value> v = o->Get(p);
        if (v->IsString()){
            ret = v->ToString();
        }
    }
    if (ret.IsEmpty()) ret = String::NewFromUtf8(iso, def);
    return scope.Escape(ret);
}

void ObjectV8::set(const char *key, bool value){
    Isolate *iso = Isolate::GetCurrent();
    HandleScope scope(iso);
    Local<Object> o = Local<Object>::New(iso,_obj);
    o->Set(String::NewFromUtf8(iso,key), Boolean::New(iso,value));
}
void ObjectV8::set(const char *key, int value){
    Isolate *iso = Isolate::GetCurrent();
    HandleScope scope(iso);
    Local<Object> o = Local<Object>::New(iso,_obj);
    o->Set(String::NewFromUtf8(iso,key), Integer::New(iso,value));
}
void ObjectV8::set(const char *key, unsigned int value){
    Isolate *iso = Isolate::GetCurrent();
    HandleScope scope(iso);
    Local<Object> o = Local<Object>::New(iso,_obj);
    o->Set(String::NewFromUtf8(iso,key), Integer::New(iso,value));
}
void ObjectV8::set(const char *key, double value){
    Isolate *iso = Isolate::GetCurrent();
    HandleScope scope(iso);
    Local<Object> o = Local<Object>::New(iso,_obj);
    o->Set(String::NewFromUtf8(iso,key), Number::New(iso,value));
}
void ObjectV8::set(const char *key, const std::string &value){
    set(key, value.c_str());
}
void ObjectV8::set(const char *key, const char *value){
    Isolate *iso = Isolate::GetCurrent();
    HandleScope scope(iso);
    Local<Object> o = Local<Object>::New(iso,_obj);
    o->Set(String::NewFromUtf8(iso,key), String::NewFromUtf8(iso,value));
}

Handle<Object> ObjectV8::toJSObject() const{
    Isolate *iso = Isolate::GetCurrent();
    HandleScope scope(iso);
    return Local<Object>::New(iso,_obj);
}
