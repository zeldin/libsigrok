	/** Get flags corresponding to a bitmask. */
	static vector<QuantityFlag>
		flags_from_mask(unsigned int mask);

	/** Get bitmask corresponding to a set of flags. */
	static QuantityFlag mask_from_flags(
		vector<QuantityFlag> flags);
