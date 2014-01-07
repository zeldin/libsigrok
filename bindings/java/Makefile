SRC := .
OUT := .

all : $(OUT)/libsigrok_bindings.jar $(OUT)/libsigrok_bindings.so

include build.mk

ifneq ($(origin JAVA_HOME),undefined)
  INCLUDES += -I$(JAVA_HOME)/include
endif

override CFLAGS += $(INCLUDES)

$(OUT)/libsigrok_bindings.so : $(SWIG_OUT)/libsigrok_wrap.c
	$(CC) $(LDFLAGS) $(CFLAGS) -fpic -shared -o $@ $< $(LIBS)

clean :
	rm -f $(OUT)/libsigrok_bindings.so $(OUT)/libsigrok_bindings.jar
	rm -rf $(SWIG_OUT) $(JAVAC_OUT)
