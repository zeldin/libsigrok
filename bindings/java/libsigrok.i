/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2013 Marcus Comstedt <marcus@mc.pp.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

%module Libsigrok

%include "arrays_java.i"
%include "typemaps.i"

%nodefaultctor;
%nodefaultdtor;

%defaultctor sr_input;
%defaultctor sr_output;
%defaultdtor sr_input;
%defaultdtor sr_output;

%rename("%(lowercamelcase)s",sourcefmt="%(strip:[sr_])s") "";
%rename("%(camelcase)s",sourcefmt="%(strip:[sr_])s",%$isclass) "";
%rename("%(strip:[SR_])s",regextarget=1) "SR_.*";

%typemap(javaclassmodifiers) SWIGTYPE "public final class"
%typemap(javaclassmodifiers) SWIGTYPE * "class"
%typemap(javaclassmodifiers) struct _GSList, struct _GString "class"
%typemap(javaclassmodifiers) float_array "class"
%ignore float_array;

%typemap(jtype, nopgcpp="1") GSList * "long"

%typemap(javaimports) struct sr_datafeed_header "import java.util.Date;"

%pragma(java) jniclassclassmodifiers="final class"
%pragma(java) moduleclassmodifiers="public final class"

%pragma(java) moduleimports="import java.util.List;"

%pragma(java) jniclasscode=%{
  static {
    try {
      System.loadLibrary("sigrok_bindings");
    } catch (UnsatisfiedLinkError e) {
      System.err.println("Native code library failed to load.\n" + e);
      System.exit(1);
    }
  }
%}

%pragma(java) modulecode=%{
  private static Context context;
  private static final Object FINAL = new Object() {
    @Override
    protected void finalize() throws Throwable {
      super.finalize();
      if (context != null) {
	exit(context);
	context = null;
      }
    }
  };
  static {
    SWIGTYPE_p_p_sr_context pContext = newSrContextPtrPtr();
    int rc = init(pContext);
    if (rc != OK) {
      System.err.println("Native code library failed to initialize.\n" +
	strerror(rc));
      System.exit(1);
    }
    context = contextPtrPtrValue(pContext);
    deleteSrContextPtrPtr(pContext);
  };
  public static Context getContext() { return context; }
%}

%immutable;
%noimmutable sr_input::format;
%noimmutable sr_input::param;
%noimmutable sr_output::format;
%noimmutable sr_output::param;

/* Free dynamic strings */
%typemap(out, noblock=1)
	char * sr_log_logdomain_get,
	char * sr_si_string_u64,
	char * sr_samplerate_string,
	char * sr_period_string,
	char * sr_voltage_string
 { if ($1) { $result = JCALL1(NewStringUTF, jenv, (const char *)$1); g_free($1); } }

/* Use long for uint64_t, to avoid BigNums */
%typemap(jni) uint64_t "jlong"
%typemap(jtype) uint64_t "long"
%typemap(jstype) uint64_t "long"
%typemap(in) uint64_t %{ $1 = ($1_ltype)$input; %}
%typemap(out) uint64_t %{ $result = ($1_ltype)$1; %}
%typemap(javain, throws="IllegalArgumentException",
	pre="    if($javainput < 0)\n"
	    "      throw new IllegalArgumentException(\"Parameter $javainput may not be negative\");") uint64_t "$javainput"

/* Other convenient prototype changes */
%typemap(jstype) struct sr_session * "Session"
%typemap(javain) struct sr_session * "$javainput.getCPtr()"
%typemap(javaout) struct sr_session * {
    return Session.makeFromCPtr($jnicall);
  }
%typemap(jstype) struct sr_context * "Context"
%typemap(javain) struct sr_context * "$javainput.getCPtr()"
%typemap(javaout) struct sr_context * {
    return Context.makeFromCPtr($jnicall);
  }

%typemap(jstype) struct sr_input_format ** sr_input_list "InputFormat[]"
%typemap(javaout) struct sr_input_format ** sr_input_list {
    return WrapperUtil.makeInputFormatArray($jnicall);
  }
%typemap(jstype) struct sr_output_format ** sr_output_list "OutputFormat[]"
%typemap(javaout) struct sr_output_format ** sr_output_list {
    return WrapperUtil.makeOutputFormatArray($jnicall);
  }
%typemap(jstype) struct sr_dev_driver ** sr_driver_list "DevDriver[]"
%typemap(javaout) struct sr_dev_driver ** sr_driver_list {
    return WrapperUtil.makeDevDriverArray($jnicall);
  }
%typemap(jstype) GSList * sr_dev_list, GSList * sr_driver_scan "DevInst[]"
%typemap(javaout) GSList * sr_dev_list, GSList * sr_driver_scan {
    return WrapperUtil.makeDevInstArray($jnicall);
  }
%ignore new_sr_input_format_ptr_array;
%ignore delete_sr_input_format_ptr_array;
%javamethodmodifiers sr_input_format_ptr_array_getitem "";
%ignore sr_input_format_ptr_array_setitem;
%ignore new_sr_output_format_ptr_array;
%ignore delete_sr_output_format_ptr_array;
%javamethodmodifiers sr_output_format_ptr_array_getitem "";
%ignore sr_output_format_ptr_array_setitem;
%ignore new_sr_dev_driver_ptr_array;
%ignore delete_sr_dev_driver_ptr_array;
%javamethodmodifiers sr_dev_driver_ptr_array_getitem "";
%ignore sr_dev_driver_ptr_array_setitem;

%typemap(jstype) GSList * options "Config[]"
%typemap(javain,
	pre="    GSList temp$javainput = null;\n"
	    "    if ($javainput != null)\n"
	    "      temp$javainput = WrapperUtil.makeConfigList($javainput);\n",
	post="    if (temp$javainput != null)\n"
	     "      gSlistFree(temp$javainput);\n") GSList * options "$javaclassname.getCPtr(temp$javainput)"

%typemap(jstype) GSList ** devlist "List<DevInst>"
%typemap(javain,
	pre="    SWIGTYPE_p_p__GSList temp$javainput = newGslistPtrPtr();",
	post="      try {\n"
	     "        WrapperUtil.fillDevInstListFromGslist($javainput, gslistPtrPtrValue(temp$javainput));\n"
	     "      } finally {\n"
	     "        if (temp$javainput != null) {\n"
	     "          GSList list = gslistPtrPtrValue(temp$javainput);\n"
	     "          if (list != null)\n"
	     "            gSlistFree(list);\n"
	     "          deleteGslistPtrPtr(temp$javainput);\n"
	     "        }\n"
	     "      };") GSList ** devlist "$javaclassname.getCPtr(temp$javainput)"

%typemap(jni) struct timeval "jlong"
%typemap(jtype) struct timeval "long"
%typemap(jstype) struct timeval "Date"
%typemap(out) struct timeval %{ $result = (((jlong)$1.tv_sec)*1000+$1.tv_usec/1000); %}
%typemap(javaout) struct timeval {
    return new Date($jnicall);
  }

%typemap(jstype) GVariant* data "Variant"
%typemap(javain) GVariant* data "Variant.getCPtr($javainput)"
%typemap(javaout) GVariant* data {
    long cPtr = $jnicall;
    return (cPtr == 0) ? null : new Variant(cPtr, $owner);
  }

%apply int8_t[] { void *sr_datafeed_logic::data };
%typemap(out) void *sr_datafeed_logic::data
%{$result = SWIG_JavaArrayOutSchar(jenv, (signed char *)$1, (arg1)->length); %}
%apply float[] { float *sr_datafeed_analog::data };
%typemap(out) float *sr_datafeed_analog::data
%{$result = SWIG_JavaArrayOutFloat(jenv, (float *)$1, (arg1)->num_samples); %}

%typemap(jstype) (uint64_t *p, uint64_t *q ) "Fraction"
%typemap(jtype) (uint64_t *p, uint64_t *q ) "long[]"
%typemap(jni) (uint64_t *p, uint64_t *q ) "jlongArray"
%typemap(javain,
         pre="    long[] arr$javainput = new long[2];",
	 post="      $javainput.setPQ(arr$javainput[0], arr$javainput[1]);") (uint64_t *p, uint64_t *q ) "arr$javainput"
%typemap(in,numinputs=1) (uint64_t *p, uint64_t *q) {
  uint64_t p$input = 0, q$input = 0;
  $1 = &p$input;
  $2 = &q$input;
}
%typemap(freearg) (uint64_t *p, uint64_t *q) %{
  jlong arr$input[] = { *$1, *$2 };
  (*jenv)->SetLongArrayRegion(jenv, $input, 0, 2, arr$input);
%}

%apply int64_t *OUTPUT { uint64_t *size };

%typemap(jstype) char **sr_parse_triggerstring "String[]"
%typemap(jtype) char **sr_parse_triggerstring "String[]"
%typemap(jni) char **sr_parse_triggerstring "jobjectArray"
%typemap(javaout) char **sr_parse_triggerstring {
  return $jnicall;
}
%typemap(out) char **sr_parse_triggerstring %{
  if ($1) {
    jclass clazz$1 = (*jenv)->FindClass(jenv, "java/lang/String");
    int i$1, cnt$1 = g_slist_length(arg1->probes);
    $result = (*jenv)->NewObjectArray(jenv, cnt$1, clazz$1, (jobject)0);
    for(i$1=0; i$1<cnt$1; i$1++)
       if($1[i$1]) {
         jobject str$1 = (jobject) (*jenv)->NewStringUTF(jenv, $1[i$1]);
         (*jenv)->SetObjectArrayElement(jenv, $result, i$1, str$1);
       }
    g_free($1);
  }
%}

%typemap(jstype) (unsigned char *buf) "byte[]"
%typemap(jtype) (unsigned char *buf) "byte[]"
%typemap(jni) (unsigned char *buf) "jbyteArray"
%typemap(javain, throws="IllegalArgumentException",
	pre="    if($javainput == null || unitsize < 1 || $javainput.length%unitsize != 0)\n"
	    "      throw new IllegalArgumentException(\"Parameters $javainput and unitsize do not match\");") (unsigned char *buf) "$javainput"
%typemap(in) (unsigned char *buf) %{
  $1 = (unsigned char *)(*jenv)->GetByteArrayElements(jenv, $input, NULL);
%}
%typemap(freearg) (unsigned char *buf) %{
  (*jenv)->ReleaseByteArrayElements(jenv, $input, $1, 0);
%}
%typemap(in,numinputs=0) (int units) %{
  jbyteArray arr$argnum = 0;
  jint unitsize$argnum = 0;
#if $argnum == 4
  arr$argnum = jarg2;
  unitsize$argnum = jarg3;
#elif $argnum == 5
  arr$argnum = jarg3;
  unitsize$argnum = jarg4;
#else
#error Array not found
#endif
  if (arr$argnum && unitsize$argnum>0)
    $1 = (*jenv)->GetArrayLength(jenv, arr$argnum) / unitsize$argnum;
  else
    $1 = 0;
%}

%typemap(jstype) char **probes "String[]"
%typemap(jtype) char **probes "String[]"
%typemap(jni) char **probes "jobjectArray"
%typemap(javain) char **probes "$javainput"
%typemap(in) char **probes %{
  if ($input) {
    jsize sz$input = (*jenv)->GetArrayLength(jenv, $input);
    $1 = calloc(sz$input+1, sizeof(char *));
    if ($1) {
      jsize i$input;
      $1[sz$input] = NULL;
      for (i$input=0; i$input<sz$input; i$input++) {
	jstring str$input =
	  (jstring)(*jenv)->GetObjectArrayElement(jenv, $input, i$input);
        if (str$input) {
	  $1[i$input] = (char *)(*jenv)->GetStringUTFChars(jenv, str$input, NULL);
          (*jenv)->DeleteLocalRef(jenv, str$input);
        }
      }
    }
  }
%}
%typemap(freearg) char **probes %{
  if ($1) {
    jsize sz$input = (*jenv)->GetArrayLength(jenv, $input);
    jsize i$input;
    for (i$input=0; i$input<sz$input; i$input++) {
      jstring str$input =
	(jstring)(*jenv)->GetObjectArrayElement(jenv, $input, i$input);
      if (str$input) {
	if ($1[i$input])
	  (*jenv)->ReleaseStringUTFChars(jenv, str$input, $1[i$input]);
	(*jenv)->DeleteLocalRef(jenv, str$input);
      }
    }
    free($1);
  }
%}

%typemap(jstype) GSList *probes "Probe[]"
%typemap(javaout) GSList *probes {
    return WrapperUtil.makeProbeArray($jnicall);
  }

/* Hide methods with "difficult" prototypes, should override with */
/* better API for those which are useful to have publicly */
%javamethodmodifiers sr_init "";
%javamethodmodifiers sr_exit "";
%javamethodmodifiers sr_filter_probes "";
%javamethodmodifiers sr_config_get "";
%javamethodmodifiers sr_config_list "";
%javamethodmodifiers sr_session_source_add "";
%javamethodmodifiers sr_session_source_add_pollfd "";
%javamethodmodifiers sr_session_source_add_channel "";
%javamethodmodifiers sr_session_source_remove_pollfd "";
%javamethodmodifiers sr_session_source_remove_channel "";
%javamethodmodifiers sr_output_format::call_event "";
%javamethodmodifiers sr_output_format::call_data "";
%javamethodmodifiers sr_output_format::call_receive "";
%javamethodmodifiers sr_datafeed_meta::config "";
%javamethodmodifiers sr_input::param "";
%javamethodmodifiers sr_dev_inst::probe_groups "";
%javamethodmodifiers sr_config::data "";

/* Hide internal thingys */
%javamethodmodifiers *::internal "";
%javamethodmodifiers *::priv "";
%javamethodmodifiers *::payload "";
%javamethodmodifiers *::conn "";
%javamethodmodifiers g_strdup "";
%javamethodmodifiers g_string_free "";
%javamethodmodifiers g_variant_new_uint64 "";
%javamethodmodifiers g_variant_new_boolean "";
%javamethodmodifiers g_variant_new_double "";
%javamethodmodifiers g_variant_new_string "";
%javamethodmodifiers g_variant_new_tuple "";
%javamethodmodifiers g_variant_get_type_string "";
%javamethodmodifiers g_variant_get_uint64 "";
%javamethodmodifiers g_variant_get_boolean "";
%javamethodmodifiers g_variant_get_double "";
%javamethodmodifiers g_variant_get_string "";
%javamethodmodifiers g_variant_get_child_value "";
%javamethodmodifiers g_hash_table_new_full "";
%javamethodmodifiers g_hash_table_insert "";
%javamethodmodifiers g_hash_table_destroy "";
%javamethodmodifiers new_uint8_ptr_ptr "";
%javamethodmodifiers copy_uint8_ptr_ptr "";
%javamethodmodifiers delete_uint8_ptr_ptr "";
%javamethodmodifiers uint8_ptr_ptr_assign "";
%javamethodmodifiers uint8_ptr_ptr_value "";
%javamethodmodifiers new_uint64_ptr "";
%javamethodmodifiers copy_uint64_ptr "";
%javamethodmodifiers delete_uint64_ptr "";
%javamethodmodifiers uint64_ptr_assign "";
%javamethodmodifiers uint64_ptr_value "";
%javamethodmodifiers new_gstring_ptr_ptr "";
%javamethodmodifiers copy_gstring_ptr_ptr "";
%javamethodmodifiers delete_gstring_ptr_ptr "";
%javamethodmodifiers gstring_ptr_ptr_assign "";
%javamethodmodifiers gstring_ptr_ptr_value "";
%javamethodmodifiers new_gslist_ptr_ptr "";
%javamethodmodifiers copy_gslist_ptr_ptr "";
%javamethodmodifiers delete_gslist_ptr_ptr "";
%javamethodmodifiers gslist_ptr_ptr_assign "";
%javamethodmodifiers gslist_ptr_ptr_value "";
%javamethodmodifiers new_gvariant_ptr_ptr "";
%javamethodmodifiers copy_gvariant_ptr_ptr "";
%javamethodmodifiers delete_gvariant_ptr_ptr "";
%javamethodmodifiers gvariant_ptr_ptr_assign "";
%javamethodmodifiers gvariant_ptr_ptr_value "";
%javamethodmodifiers new_gvariant_ptr_array "";
%javamethodmodifiers delete_gvariant_ptr_array "";
%javamethodmodifiers gvariant_ptr_array_getitem "";
%javamethodmodifiers gvariant_ptr_array_setitem "";
%javamethodmodifiers new_sr_context_ptr_ptr "";
%javamethodmodifiers copy_sr_context_ptr_ptr "";
%javamethodmodifiers delete_sr_context_ptr_ptr "";
%javamethodmodifiers sr_context_ptr_ptr_assign "";
%javamethodmodifiers sr_context_ptr_ptr_value "";
%javamethodmodifiers gpointer_to_sr_dev_inst_ptr "";
%javamethodmodifiers void_ptr_to_sr_datafeed_logic_ptr "";
%javamethodmodifiers void_ptr_to_sr_datafeed_analog_ptr "";
%javamethodmodifiers void_ptr_to_sr_probe_ptr "";
%javamethodmodifiers void_ptr_to_sr_probe_group_ptr "";
%javamethodmodifiers g_slist_free "";
%javamethodmodifiers g_slist_prepend "";
%javamethodmodifiers _GString::str "";
%javamethodmodifiers _GString::len "";
%javamethodmodifiers _GString::allocated_len "";
%javamethodmodifiers _GSList::data "";
%javamethodmodifiers _GSList::next "";
%javamethodmodifiers float_array "";
%javamethodmodifiers float_array::getitem "";
%javamethodmodifiers float_array::setitem "";
%javamethodmodifiers float_array::cast "";
%javamethodmodifiers float_array::frompointer "";

/* Hide function pointers */
%javamethodmodifiers sr_input_format::format_match "";
%javamethodmodifiers sr_input_format::init "";
%javamethodmodifiers sr_input_format::loadfile "";
%javamethodmodifiers sr_output_format::init "";
%javamethodmodifiers sr_output_format::data "";
%javamethodmodifiers sr_output_format::event "";
%javamethodmodifiers sr_output_format::receive "";
%javamethodmodifiers sr_output_format::cleanup "";
%javamethodmodifiers sr_dev_driver::init "";
%javamethodmodifiers sr_dev_driver::cleanup "";
%javamethodmodifiers sr_dev_driver::scan "";
%javamethodmodifiers sr_dev_driver::dev_list "";
%javamethodmodifiers sr_dev_driver::dev_clear "";
%javamethodmodifiers sr_dev_driver::config_get "";
%javamethodmodifiers sr_dev_driver::config_set "";
%javamethodmodifiers sr_dev_driver::config_list "";
%javamethodmodifiers sr_dev_driver::dev_open "";
%javamethodmodifiers sr_dev_driver::dev_close "";
%javamethodmodifiers sr_dev_driver::dev_acquisition_start "";
%javamethodmodifiers sr_dev_driver::dev_acquisition_stop "";

/* Ignore some stuff that would clog up the LibsigrokConstants interface */
%ignore g_str_hash;
%ignore g_str_equal;
%ignore g_free;

/* DatafeedPacket */

%typemap(javacode) struct sr_datafeed_packet %{
  public DatafeedHeader getHeaderPayload() {
    if (getType() != Libsigrok.DF_HEADER)
      return null;
    long cPtr = SWIGTYPE_p_void.getCPtr(getPayload());
    return (cPtr == 0? null : new DatafeedHeader(cPtr, false));
  };
  public DatafeedMeta getMetaPayload() {
    if (getType() != Libsigrok.DF_META)
      return null;
    long cPtr = SWIGTYPE_p_void.getCPtr(getPayload());
    return (cPtr == 0? null : new DatafeedMeta(cPtr, false));
  };
  public DatafeedLogic getLogicPayload() {
    if (getType() != Libsigrok.DF_LOGIC)
      return null;
    long cPtr = SWIGTYPE_p_void.getCPtr(getPayload());
    return (cPtr == 0? null : new DatafeedLogic(cPtr, false));
  };
  public DatafeedAnalog getAnalogPayload() {
    if (getType() != Libsigrok.DF_ANALOG)
      return null;
    long cPtr = SWIGTYPE_p_void.getCPtr(getPayload());
    return (cPtr == 0? null : new DatafeedAnalog(cPtr, false));
  };
%}

/* Callbacks */

%typemap(jstype) sr_log_callback_t "LogCallback";
%typemap(jtype) sr_log_callback_t "LogCallback";
%typemap(jni) sr_log_callback_t "jobject";
%typemap(javain) sr_log_callback_t "$javainput";
%typemap(in,numinputs=1) (sr_log_callback_t cb, void *cb_data) {
  struct callback_data *data = malloc(sizeof *data);
  data->env = jenv;
  data->obj = JCALL1(NewGlobalRef, jenv, $input);
  $1 = sr_log_callback_java;
  $2 = data;
}
%typemap(jstype) sr_datafeed_callback_t "DatafeedCallback";
%typemap(jtype) sr_datafeed_callback_t "DatafeedCallback";
%typemap(jni) sr_datafeed_callback_t "jobject";
%typemap(javain) sr_datafeed_callback_t "$javainput";
%typemap(in,numinputs=1) (sr_datafeed_callback_t cb, void *cb_data) {
  struct callback_data *data = malloc(sizeof *data);
  data->env = jenv;
  data->obj = JCALL1(NewGlobalRef, jenv, $input);
  $1 = sr_datafeed_callback_java;
  $2 = data;
}

%include "../swig/libsigrok.i"

/* Code for callbacks */

%{
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

struct callback_data {
  JNIEnv *env;
  jobject obj;
};

int sr_log_callback_java(void *cb_data, int loglevel,
                         const char *format, va_list args)
{
  jclass callbackInterfaceClass;
  jmethodID meth;
  jstring str = 0;
  char *buf = NULL;
  jint result = 0;
  struct callback_data *data = cb_data;
  callbackInterfaceClass = (*data->env)->FindClass(data->env,
	 "org/sigrok/libsigrok/LogCallback");
  assert(callbackInterfaceClass);
  meth = (*data->env)->GetMethodID(data->env, callbackInterfaceClass, "handle", "(ILjava/lang/String;)I");
  assert(meth);
  if (vasprintf(&buf, format, args) >= 0) {
    str = (*data->env)->NewStringUTF(data->env, buf);
    free(buf);
  }
  result = (*data->env)->CallIntMethod(data->env, data->obj, meth, (jint)loglevel, (jstring)str);
  if(str)
    (*data->env)->DeleteLocalRef(data->env, str);
  return (int)result;
}

void sr_datafeed_callback_java(const struct sr_dev_inst *sdi,
			       const struct sr_datafeed_packet *packet,
			       void *cb_data)
{
  jlong arg1 = 0;
  jlong arg2 = 0;
  jobject arg1_ = 0;
  jobject arg2_ = 0;
  jclass callbackInterfaceClass, wrapperClass;
  jmethodID meth;
  struct callback_data *data = cb_data;
  *(const struct sr_dev_inst **)&arg1 = sdi;
  *(const struct sr_datafeed_packet **)&arg2 = packet;
  callbackInterfaceClass = (*data->env)->FindClass(data->env,
	 "org/sigrok/libsigrok/DatafeedCallback");
  wrapperClass = (*data->env)->FindClass(data->env,
	 "org/sigrok/libsigrok/WrapperUtil");
  assert(callbackInterfaceClass);
  assert(wrapperClass);
  meth = (*data->env)->GetStaticMethodID(data->env, wrapperClass, "makeDevInst", "(J)Lorg/sigrok/libsigrok/DevInst;");
  assert(meth);
  arg1_ = (*data->env)->CallStaticObjectMethod(data->env, wrapperClass, meth, (jlong)arg1);
  meth = (*data->env)->GetStaticMethodID(data->env, wrapperClass, "makeDatafeedPacket", "(J)Lorg/sigrok/libsigrok/DatafeedPacket;");
  assert(meth);
  arg2_ = (*data->env)->CallStaticObjectMethod(data->env, wrapperClass, meth, (jlong)arg2);
  meth = (*data->env)->GetMethodID(data->env, callbackInterfaceClass, "handle", "(Lorg/sigrok/libsigrok/DevInst;Lorg/sigrok/libsigrok/DatafeedPacket;)V");
  assert(meth);
  (*data->env)->CallVoidMethod(data->env, data->obj, meth, (jobject)arg1_, (jobject)arg2_);
  (*data->env)->DeleteLocalRef(data->env, arg1_);
  (*data->env)->DeleteLocalRef(data->env, arg2_);
}
%}

