import java.util.Hashtable;
import java.util.BitSet;

import com.ibm.icu.text.UTF16;

/**
 * Accesses the Normalization Data used for Forms C and D.<br>
 * Copyright (c) 1991-2005 Unicode, Inc.
 * For terms of use, see http://www.unicode.org/terms_of_use.html
 * For documentation, see UAX#15.<br>
 * @author Mark Davis
 */
public class NormalizerData {
    static final String copyright = "Copyright © 1998-1999 Unicode, Inc.";
    
    /**
    * Constant for use in getPairwiseComposition
    */
    public static final int NOT_COMPOSITE = '\uFFFF';

    /**
    * Gets the combining class of a character from the
    * Unicode Character Database.
    * @param   ch      the source character
    * @return          value from 0 to 255
    */
    public int getCanonicalClass(int ch) {
        return canonicalClass.get(ch);
    }

    /**
    * Returns the composite of the two characters. If the two
    * characters don't combine, returns NOT_COMPOSITE.
    * Only has to worry about BMP characters, since those are the only ones that can ever compose.
    * @param   first   first character (e.g. 'c')
    * @param   first   second character (e.g. '¸' cedilla)
    * @return          composite (e.g. 'ç')
    */
    public char getPairwiseComposition(int first, int second) {
    	if (first < 0 || first > 0x10FFFF || second < 0 || second > 0x10FFFF) return NOT_COMPOSITE;
        return (char)compose.get((first << 16) | second);
    }

    /**
    * Gets recursive decomposition of a character from the 
    * Unicode Character Database.
    * @param   canonical    If true
    *                  bit is on in this byte, then selects the recursive 
    *                  canonical decomposition, otherwise selects
    *                  the recursive compatibility and canonical decomposition.
    * @param   ch      the source character
    * @param   buffer  buffer to be filled with the decomposition
    */
    public void getRecursiveDecomposition(boolean canonical, int ch, StringBuffer buffer) {
        String decomp = decompose.get(ch); 
        if (decomp != null && !(canonical && isCompatibility.get(ch))) {
            for (int i = 0; i < decomp.length(); ++i) {
                getRecursiveDecomposition(canonical, decomp.charAt(i), buffer);
            }
        } else {                    // if no decomp, append
        	UTF16.append(buffer, ch);
        }
    }
    
    // =================================================
    //                   PRIVATES
    // =================================================
    
    /**
     * Only accessed by NormalizerBuilder.
     */
    NormalizerData(IntHashtable canonicalClass, IntStringHashtable decompose, 
      IntHashtable compose, BitSet isCompatibility, BitSet isExcluded) {
        this.canonicalClass = canonicalClass;
        this.decompose = decompose;
        this.compose = compose;
        this.isCompatibility = isCompatibility;
        this.isExcluded = isExcluded;
    }
    
    /**
    * Just accessible for testing.
    */
    boolean getExcluded (char ch) {
        return isExcluded.get(ch);
    }
   
    /**
    * Just accessible for testing.
    */
    String getRawDecompositionMapping (char ch) {
        return decompose.get(ch);
    }
   
    /**
    * For now, just use IntHashtable
    * Two-stage tables would be used in an optimized implementation.
    */
    private IntHashtable canonicalClass;

    /**
    * The main data table maps chars to a 32-bit int.
    * It holds either a pair: top = first, bottom = second
    * or singleton: top = 0, bottom = single.
    * If there is no decomposition, the value is 0.
    * Two-stage tables would be used in an optimized implementation.
    * An optimization could also map chars to a small index, then use that
    * index in a small array of ints.
    */
    private IntStringHashtable decompose;
    
    /**
    * Maps from pairs of characters to single.
    * If there is no decomposition, the value is NOT_COMPOSITE.
    */
    private IntHashtable compose;
    
    /**
    * Tells whether decomposition is canonical or not.
    */
    private BitSet isCompatibility = new BitSet();
    
    /**
    * Tells whether character is script-excluded or not.
    * Used only while building, and for testing.
    */
    
    private BitSet isExcluded = new BitSet();
}