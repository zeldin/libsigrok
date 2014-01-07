package org.sigrok.libsigrok;

public interface DatafeedCallback
{
    public void handle(DevInst sdi, DatafeedPacket packet);
};
