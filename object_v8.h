#ifndef _OBJECT_V8_H_
#define _OBJECT_V8_H_

#include <string>

class ObjectV8{
public:
    ObjectV8();
    ObjectV8(v8::Handle<v8::Object> obj);
    ~ObjectV8();

    bool get(const char *key, bool def) const;
    int get(const char *key, int def) const;
    unsigned int get(const char *key, unsigned int def) const;
    double get(const char *key, double def) const;
    v8::Handle<v8::String> get(const char *key, const char *def) const;

    void set(const char *key, bool value);
    void set(const char *key, int value);
    void set(const char *key, unsigned int value);
    void set(const char *key, double value);
    void set(const char *key, const std::string &value);
    void set(const char *key, const char *value);

    v8::Handle<v8::Object> toJSObject() const;

private:
    v8::Persistent<v8::Object> _obj;
};

const char* toUTF8(v8::Handle<v8::String> jsStr, char *buffer);

#endif // _OBJECT_V8_H_
