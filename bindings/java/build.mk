SWIG_OUT := $(OUT)/swig_out
JAVAC_OUT := $(OUT)/javac_out

INCLUDES = `pkg-config --cflags libsigrok`
LIBS = `pkg-config --libs libsigrok`

SWIGFLAGS = -package org.sigrok.libsigrok $(INCLUDES)

JAVA_SRC = source/Session.java source/Context.java \
           source/WrapperUtil.java source/Variant.java \
           source/LogCallback.java source/DatafeedCallback.java \
           source/Fraction.java

-include $(SWIG_OUT)/libsigrok_wrap.c.deps

$(SWIG_OUT)/libsigrok_wrap.c : $(SRC)/libsigrok.i
	@test -d $(SWIG_OUT) || mkdir $(SWIG_OUT)
	swig $(SWIGFLAGS) -java -outdir $(SWIG_OUT) -MMD -MF $@.deps -o $@ $<

$(OUT)/libsigrok_bindings.jar : $(SWIG_OUT)/libsigrok_wrap.c $(JAVA_SRC)
	@test -d $(JAVAC_OUT) || mkdir $(JAVAC_OUT)
	javac -d $(JAVAC_OUT) $(SWIG_OUT)/*.java $(JAVA_SRC)
	jar cf $@ -C $(JAVAC_OUT) .
