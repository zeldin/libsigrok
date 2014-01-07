package org.sigrok.libsigrok;

public final class Context {

    protected long contextCPtr;

    static Context makeFromCPtr(long cPtr)
    {
	return (cPtr == 0) ? null : new Context(cPtr);
    }

    long getCPtr() {
	return contextCPtr;
    }

    Context(long cPtr)
    {
	contextCPtr = cPtr;
    }

}
