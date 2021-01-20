%module classes

/* Automatically load JNI library. */
%pragma(java) jniclasscode=%{
  static {
    System.loadLibrary("sigrok_java_core_classes");
  }
%}

/* Documentation & importing interfaces. */
%pragma(java) jniclassimports=%{
/**
 * @mainpage API Reference
 *
 * Introduction
 * ------------
 *
 * The sigrok-java API provides an object-oriented Java interface to the
 * functionality in libsigrok. It is built on top of the libsigrokcxx C++ API.
 *
 * Getting started
 * ---------------
 *
 * Usage of the sigrok-java API needs to begin with a call to Context.create().
 * This will create the global libsigrok context and returns a Context object.
 * Methods on this object provide access to the hardware drivers, input and
 * output formats supported by the library, as well as means of creating other
 * objects such as sessions and triggers.
 *
 * Error handling
 * --------------
 *
 * When any libsigrok C API call returns an error, an Error exception is raised,
 * which provides access to the error code and description.
 */

import org.sigrok.core.interfaces.LogCallback;
import org.sigrok.core.interfaces.DatafeedCallback;
%}

/* Map Glib::VariantBase to a Variant class in Java */
%rename(Variant) VariantBase;
namespace Glib {
  class VariantBase {};
}

%include "bindings/swig/templates.i"

/* Map between std::vector and java.util.Vector */
%define VECTOR(CValue, JValue)

%typemap(jni) std::vector< CValue > "jobject"
%typemap(jtype) std::vector< CValue > "java.util.Vector<JValue>"
%typemap(jstype) std::vector< CValue > "java.util.Vector<JValue>"

%typemap(javain,
    pre="  $javaclassname temp$javainput = new $javaclassname();
  for (JValue value : $javainput)
    temp$javainput.add(value);",
    pgcppname="temp$javainput")
  std::vector< CValue > "$javaclassname.getCPtr(temp$javainput)"

%typemap(javaout) std::vector< CValue > {
  return (java.util.Vector<JValue>)$jnicall;
}

%typemap(out) std::vector< CValue > {
  jclass Vector = jenv->FindClass("java/util/Vector");
  jmethodID Vector_init = jenv->GetMethodID(Vector, "<init>", "()V");
  jmethodID Vector_add = jenv->GetMethodID(Vector, "add",
    "(Ljava/lang/Object;)Z");
  jclass Value = jenv->FindClass("org/sigrok/core/classes/" #JValue);
  jmethodID Value_init = jenv->GetMethodID(Value, "<init>", "(JZ)V");
  $result = jenv->NewObject(Vector, Vector_init);
  jlong value = 0;
  for (auto entry : $1)
  {
    *(CValue **) &value = new CValue(entry);
    jenv->CallBooleanMethod($result, Vector_add,
      jenv->NewObject(Value, Value_init, value, true));
  }
}

%enddef

VECTOR(std::shared_ptr<sigrok::Channel>, Channel)
VECTOR(std::shared_ptr<sigrok::HardwareDevice>, HardwareDevice)

/* Common macro for mapping between std::map and java.util.Map */

%define MAP_COMMON(CKey, CValue, JKey, JValue)

%typemap(jstype) std::map< CKey, CValue >
  "java.util.Map<JKey, JValue>"

%typemap(javain,
/* SWIG 4.0.0 changed the std::map wrappers in an incompatible way. */
#if SWIG_VERSION >= 0x040000
    pre="  $javaclassname temp$javainput = new $javaclassname();
    for (java.util.Map.Entry<JKey, JValue> entry : $javainput.entrySet())
      temp$javainput.put(entry.getKey(), entry.getValue());",
    pgcppname="temp$javainput")
#else
    pre="  $javaclassname temp$javainput = new $javaclassname();
    for (java.util.Map.Entry<JKey, JValue> entry : $javainput.entrySet())
      temp$javainput.set(entry.getKey(), entry.getValue());",
    pgcppname="temp$javainput")
#endif
  std::map< CKey, CValue > "$javaclassname.getCPtr(temp$javainput)"

%typemap(javaout) std::map< CKey, CValue > {
  return (java.util.Map<JKey, JValue>)$jnicall;
}

%enddef

/* Specialisation for string->string maps. */

MAP_COMMON(std::string, std::string, String, String)

%typemap(jni) std::map<std::string, std::string>
  "jobject"
%typemap(jtype) std::map<std::string, std::string>
  "java.util.Map<String,String>"

%typemap(out) std::map<std::string, std::string> {
  jclass HashMap = jenv->FindClass("java/util/HashMap");
  jmethodID init = jenv->GetMethodID(HashMap, "<init>", "()V");
  jmethodID put = jenv->GetMethodID(HashMap, "put",
    "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
  $result = jenv->NewObject(HashMap, init);
  for (auto entry : $1)
    jenv->CallObjectMethod($result, put,
      jenv->NewStringUTF(entry.first.c_str()),
      jenv->NewStringUTF(entry.second.c_str()));
}

/* Specialisation macro for string->shared_ptr maps. */

%define STRING_TO_SHARED_PTR_MAP(ClassName)

%typemap(jni) std::map<std::string, std::shared_ptr<sigrok::ClassName> >
  "jobject"
%typemap(jtype) std::map<std::string, std::shared_ptr<sigrok::ClassName> >
  "java.util.Map<String,ClassName>"

MAP_COMMON(std::string, std::shared_ptr<sigrok::ClassName>, String, ClassName)

%typemap(out) std::map<std::string, std::shared_ptr<sigrok::ClassName> > {
  jclass HashMap = jenv->FindClass("java/util/HashMap");
  jmethodID HashMap_init = jenv->GetMethodID(HashMap, "<init>", "()V");
  jmethodID HashMap_put = jenv->GetMethodID(HashMap, "put",
    "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
  jclass Value = jenv->FindClass("org/sigrok/core/classes/" #ClassName);
  jmethodID Value_init = jenv->GetMethodID(Value, "<init>", "(JZ)V");
  $result = jenv->NewObject(HashMap, HashMap_init);
  jlong value = 0;
  for (auto entry : $1)
  {
    *(std::shared_ptr< sigrok::ClassName > **)&value =
      new std::shared_ptr< sigrok::ClassName>(entry.second);
    jenv->CallObjectMethod($result, HashMap_put,
      jenv->NewStringUTF(entry.first.c_str()),
      jenv->NewObject(Value, Value_init, value, true));
  }
}

%enddef

STRING_TO_SHARED_PTR_MAP(Driver)
STRING_TO_SHARED_PTR_MAP(InputFormat)
STRING_TO_SHARED_PTR_MAP(OutputFormat)

/* Specialisation for ConfigKey->Variant maps */

MAP_COMMON(const sigrok::ConfigKey *, Glib::VariantBase, ConfigKey, Variant)

%typemap(jni) std::map<const sigrok::ConfigKey, Glib::VariantBase> "jobject"
%typemap(jtype) std::map<const sigrok::ConfigKey, Glib::VariantBase>
  "java.util.Map<ConfigKey,Variant>"

%typemap(out) std::map<const sigrok::ConfigKey *, Glib::VariantBase> {
  jclass HashMap = jenv->FindClass("java/util/HashMap");
  jmethodID HashMap_init = jenv->GetMethodID(HashMap, "<init>", "()V");
  jmethodID HashMap_put = jenv->GetMethodID(HashMap, "put",
    "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
  jclass ConfigKey = jenv->FindClass("org/sigrok/core/classes/ConfigKey");
  jmethodID ConfigKey_init = jenv->GetMethodID(ConfigKey, "<init>", "(JZ)V");
  jclass Variant = jenv->FindClass("org/sigrok/core/classes/Variant");
  jmethodID Variant_init = jenv->GetMethodID(Variant, "<init>", "(JZ)V");
  $result = jenv->NewObject(HashMap, HashMap_init);
  jlong key = 0;
  jlong value = 0;
  for (auto entry : $1)
  {
    *(const sigrok::ConfigKey **) &key = entry.first;
    *(Glib::VariantBase **) &value = new Glib::VariantBase(entry.second);
    jenv->CallObjectMethod($result, HashMap_put,
      jenv->NewObject(ConfigKey, ConfigKey_init, key, false));
      jenv->NewObject(Variant, Variant_init, value, true));
  }
}

/* Pass JNIEnv parameter to C++ extension methods requiring it. */

%typemap(in, numinputs=0) JNIEnv * %{
   $1 = jenv;
%}

/* Thread safe JNIEnv handling */

%inline {
namespace {
  class ScopedEnv {
    public:
      ScopedEnv(JavaVM *jvm);
      ~ScopedEnv();
      JNIEnv* operator-> () { return env; }
      operator bool () const { return (bool)env; }
    private:
      JavaVM *jvm;
      JNIEnv *env;
      int env_status;
  };
  ScopedEnv::ScopedEnv(JavaVM *jvm) : jvm(jvm), env(NULL) {
    env_status = jvm->GetEnv((void **)&env, JNI_VERSION_1_2);
    if (env_status == JNI_EDETACHED) {
%#if defined(__ANDROID__)
      jvm->AttachCurrentThread(&env, NULL);
%#else
      jvm->AttachCurrentThread((void **)&env, NULL);
%#endif
    }
  }
  ScopedEnv::~ScopedEnv() {
    if (env_status == JNI_EDETACHED) {
      jvm->DetachCurrentThread();
    }
  }
}
}

/* "Smartpointer" for Java references. */

%inline {
namespace {
  class GlobalRefBase
  {
    protected:
      GlobalRefBase (JavaVM *jvm, jobject ref);
      ~GlobalRefBase ();
      JavaVM *jvm;
      jobject jref;
  };
  GlobalRefBase::GlobalRefBase (JavaVM *jvm, jobject ref) : jvm(jvm), jref(0) {
    ScopedEnv env(jvm);
    if (env && ref)
      jref = env->NewGlobalRef(ref);
  }
  GlobalRefBase::~GlobalRefBase () {
    ScopedEnv env(jvm);
    if(env && jref)
      env->DeleteGlobalRef(jref);
  }
  template <class Jtype>
  class GlobalRef : private GlobalRefBase
  {
    public:
      GlobalRef (JavaVM *jvm, Jtype ref) : GlobalRefBase(jvm, ref) {}
      GlobalRef (const GlobalRef &ref) : GlobalRefBase(ref.jvm, ref.jref) {}
      operator Jtype () const { return static_cast<Jtype>(jref); }
  };
}
}

/* Support Java log callbacks. */

%typemap(javaimports) sigrok::Context
  "import org.sigrok.core.interfaces.LogCallback;"

%inline {
typedef jobject jlogcallback;
}

%typemap(jni) jlogcallback "jlogcallback"
%typemap(jtype) jlogcallback "LogCallback"
%typemap(jstype) jlogcallback "LogCallback"
%typemap(javain) jlogcallback "$javainput"

%extend sigrok::Context
{
  void add_log_callback(JNIEnv *env, jlogcallback obj)
  {
    JavaVM *jvm = NULL;
    env->GetJavaVM(&jvm);
    jclass obj_class = env->GetObjectClass(obj);
    jmethodID method = env->GetMethodID(obj_class, "run",
      "(Lorg/sigrok/core/classes/LogLevel;Ljava/lang/String;)V");
    GlobalRef<jclass> LogLevel(jvm, env->FindClass("org/sigrok/core/classes/LogLevel"));
    jmethodID LogLevel_init = env->GetMethodID(LogLevel, "<init>", "(JZ)V");
    GlobalRef<jobject> obj_ref(jvm, obj);

    $self->set_log_callback([=] (
      const sigrok::LogLevel *loglevel,
      std::string message)
    {
      ScopedEnv env(jvm);
      if (!env)
        throw sigrok::Error(SR_ERR);
      jlong loglevel_addr = 0;
      *(const sigrok::LogLevel **) &loglevel_addr = loglevel;
      jobject loglevel_obj = env->NewObject(
        LogLevel, LogLevel_init, loglevel_addr, false);
      jobject message_obj = env->NewStringUTF(message.c_str());
      env->CallVoidMethod(obj_ref, method, loglevel_obj, message_obj);
      if (env->ExceptionCheck())
        throw sigrok::Error(SR_ERR);
    });
  }
}

/* Support Java datafeed callbacks. */

%typemap(javaimports) sigrok::Session
  "import org.sigrok.core.interfaces.DatafeedCallback;"

%inline {
typedef jobject jdatafeedcallback;
}

%typemap(jni) jdatafeedcallback "jdatafeedcallback"
%typemap(jtype) jdatafeedcallback "DatafeedCallback"
%typemap(jstype) jdatafeedcallback "DatafeedCallback"
%typemap(javain) jdatafeedcallback "$javainput"

%extend sigrok::Session
{
  void add_datafeed_callback(JNIEnv *env, jdatafeedcallback obj)
  {
    JavaVM *jvm = NULL;
    env->GetJavaVM(&jvm);
    jclass obj_class = env->GetObjectClass(obj);
    jmethodID method = env->GetMethodID(obj_class, "run",
      "(Lorg/sigrok/core/classes/Device;Lorg/sigrok/core/classes/Packet;)V");
    GlobalRef<jclass> Device(jvm, env->FindClass("org/sigrok/core/classes/Device"));
    jmethodID Device_init = env->GetMethodID(Device, "<init>", "(JZ)V");
    GlobalRef<jclass> Packet(jvm, env->FindClass("org/sigrok/core/classes/Packet"));
    jmethodID Packet_init = env->GetMethodID(Packet, "<init>", "(JZ)V");
    GlobalRef<jobject> obj_ref(jvm, obj);

    $self->add_datafeed_callback([=] (
      std::shared_ptr<sigrok::Device> device,
      std::shared_ptr<sigrok::Packet> packet)
    {
      ScopedEnv env(jvm);
      if (!env)
        throw sigrok::Error(SR_ERR);
      jlong device_addr = 0;
      jlong packet_addr = 0;
      *(std::shared_ptr<sigrok::Device> **) &device_addr =
        new std::shared_ptr<sigrok::Device>(device);
      *(std::shared_ptr<sigrok::Packet> **) &packet_addr =
        new std::shared_ptr<sigrok::Packet>(packet);
      jobject device_obj = env->NewObject(
        Device, Device_init, device_addr, true);
      jobject packet_obj = env->NewObject(
        Packet, Packet_init, packet_addr, true);
      env->CallVoidMethod(obj_ref, method, device_obj, packet_obj);
      if (env->ExceptionCheck())
        throw sigrok::Error(SR_ERR);
    });
  }
}

%include "doc.i"

%define %enumextras(Class)
%enddef

/* Ignore these for now, need fixes. */
%ignore sigrok::Context::create_analog_packet;
%ignore sigrok::Context::create_meta_packet;
%ignore sigrok::Meta::config;

%include "bindings/swig/classes.i"

