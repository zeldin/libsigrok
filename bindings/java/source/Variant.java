package org.sigrok.libsigrok;

public final class Variant {
    private SWIGTYPE_p_GVariant variant;

    public Variant(long val) {
	if (val < 0)
	    throw new
		IllegalArgumentException("Uint64 variant may not be negative");
	variant = Libsigrok.gVariantNewUint64(val);
    }

    public Variant(boolean val) {
	variant = Libsigrok.gVariantNewBoolean(val? 1 : 0);
    }

    public Variant(double val) {
	variant = Libsigrok.gVariantNewDouble(val);
    }

    protected static long getCPtr(Variant obj) {
	return (obj == null? 0 : SWIGTYPE_p_GVariant.getCPtr(obj.variant));
    }

    protected Variant(long cPtr, boolean futureUse) {
	variant = new SWIGTYPE_p_GVariant(cPtr, futureUse);
    }
}
