package org.sigrok.core.classes;

abstract class Enum implements java.io.Serializable
{
    protected final int swigCPtr;

    protected Enum(int ordinal)
    {
	this.swigCPtr = ordinal;
    }

    protected Object clone() throws CloneNotSupportedException
    {
	throw new CloneNotSupportedException();
    }

    public boolean equals(Object o)
    {
	return o.getClass() == this.getClass() &&
	    ((Enum)o).swigCPtr == this.swigCPtr;
    }

    public int hashCode()
    {
	return swigCPtr;
    }

    public int ordinal()
    {
	return swigCPtr;
    }

    public abstract String name();

    public String toString()
    {
	return name();
    }
}
