package org.sigrok.libsigrok;

public final class Session {

    private long sessionCPtr;

    static Session makeFromCPtr(long cPtr)
    {
	return (cPtr == 0) ? null : new Session(cPtr);
    }

    long getCPtr() {
	return sessionCPtr;
    }

    Session(long cPtr)
    {
	sessionCPtr = cPtr;
    }

}
