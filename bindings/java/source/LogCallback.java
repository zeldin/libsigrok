package org.sigrok.libsigrok;

public interface LogCallback
{
    public int handle(int loglevel, String text);
};
