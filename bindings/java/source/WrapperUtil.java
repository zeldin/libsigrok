package org.sigrok.libsigrok;

import java.util.Vector;
import java.util.List;

class WrapperUtil {

    static InputFormat[] makeInputFormatArray(long cPtr)
    {
	if (cPtr == 0)
	    return null;
	SWIGTYPE_p_p_sr_input_format ifl =
	    new SWIGTYPE_p_p_sr_input_format(cPtr, false);
	Vector<InputFormat> vec = new Vector<InputFormat>();
	for (int index = 0; ; index++) {
	    InputFormat ifo =
		Libsigrok.inputFormatPtrArrayGetitem(ifl, index);
	    if (ifo == null)
		break;
	    vec.add(ifo);
	}
	return vec.toArray(new InputFormat[vec.size()]);
    }

    static OutputFormat[] makeOutputFormatArray(long cPtr)
    {
	if (cPtr == 0)
	    return null;
	SWIGTYPE_p_p_sr_output_format ofl =
	    new SWIGTYPE_p_p_sr_output_format(cPtr, false);
	Vector<OutputFormat> vec = new Vector<OutputFormat>();
	for (int index = 0; ; index++) {
	    OutputFormat ofo =
		Libsigrok.outputFormatPtrArrayGetitem(ofl, index);
	    if (ofo == null)
		break;
	    vec.add(ofo);
	}
	return vec.toArray(new OutputFormat[vec.size()]);
    }

    static DevDriver[] makeDevDriverArray(long cPtr)
    {
	if (cPtr == 0)
	    return null;
	SWIGTYPE_p_p_sr_dev_driver ddl =
	    new SWIGTYPE_p_p_sr_dev_driver(cPtr, false);
	Vector<DevDriver> vec = new Vector<DevDriver>();
	for (int index = 0; ; index++) {
	    DevDriver ddo =
		Libsigrok.devDriverPtrArrayGetitem(ddl, index);
	    if (ddo == null)
		break;
	    vec.add(ddo);
	}
	return vec.toArray(new DevDriver[vec.size()]);
    }

    static DevInst[] makeDevInstArray(long cPtr)
    {
	GSList dil = (cPtr == 0? null : new GSList(cPtr, false));
	Vector<DevInst> vec = new Vector<DevInst>();
	while (dil != null) {
	    long data = SWIGTYPE_p_void.getCPtr(dil.getData());
	    DevInst dio = (data == 0? null : new DevInst(data, false));
	    vec.add(dio);
	    dil = dil.getNext();
	}
	return vec.toArray(new DevInst[vec.size()]);
    }

    static Probe[] makeProbeArray(long cPtr)
    {
	GSList pl = (cPtr == 0? null : new GSList(cPtr, false));
	Vector<Probe> vec = new Vector<Probe>();
	while (pl != null) {
	    long data = SWIGTYPE_p_void.getCPtr(pl.getData());
	    Probe po = (data == 0? null : new Probe(data, false));
	    vec.add(po);
	    pl = pl.getNext();
	}
	return vec.toArray(new Probe[vec.size()]);
    }

    static GSList makeConfigList(Config[] ca)
    {
	GSList l = null;
	try {
	    for (int i=ca.length-1; i>=0; --i) {
		SWIGTYPE_p_void data =
		    (ca[i] == null? null :
		     new SWIGTYPE_p_void(Config.getCPtr(ca[i]), false));
		l = Libsigrok.gSlistPrepend(l, data);
	    }
	    GSList r = l;
	    l = null;
	    return r;
	} finally {
	    if (l != null)
		Libsigrok.gSlistFree(l);
	}
    }

    static void fillDevInstListFromGslist(List<DevInst> list, GSList gslist)
    {
	list.clear();
	while(gslist != null) {
	    long data = SWIGTYPE_p_void.getCPtr(gslist.getData());
	    DevInst di = (data == 0? null : new DevInst(data, false));
	    list.add(di);
	    gslist = gslist.getNext();
	}
    }

    static DevInst makeDevInst(long cPtr) {
	return (cPtr == 0) ? null : new DevInst(cPtr, false);
    }

    static DatafeedPacket makeDatafeedPacket(long cPtr) {
	return (cPtr == 0) ? null : new DatafeedPacket(cPtr, false);
    }
};

