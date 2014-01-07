package org.sigrok.libsigrok;

public class Fraction extends Number implements Comparable<Fraction> {

    protected long p, q;

    public double doubleValue()
    {
	return (q == 0? 0.0d : ((double)p)/((double)q));
    }

    public float floatValue()
    {
	return (q == 0? 0.0f : ((float)p)/((float)q));
    }

    public int intValue()
    {
	return (q == 0? 0 : (int)(p/q));
    }

    public long longValue()
    {
	return (q == 0? 0 : p/q);
    }

    public long pValue()
    {
	return p;
    }

    public long qValue()
    {
	return q;
    }

    public int compareTo(Fraction anotherFraction)
    {
	long diff = p*anotherFraction.q - anotherFraction.p*q;
	return (diff < 0? -1 : (diff > 0? 1 : 0));
    }

    protected void setPQ(long p, long q)
    {
	this.p = p;
	this.q = q;
    }

    public Fraction(long p, long q)
    {
	if (q<0) {
	    p = -p;
	    q = -q;
	}
	if (p<0)
	    throw new IllegalArgumentException("Negative fractions not supported");
	setPQ(p, q);
    }

    public Fraction()
    {
	this(0, 0);
    }
}
