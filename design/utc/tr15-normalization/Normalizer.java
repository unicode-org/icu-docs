import com.ibm.icu.text.UTF16;
import com.ibm.text.utility.Utility;

/**
 * Implements Unicode Normalization Forms C, D, KC, KD.<br>
 * Copyright (c) 1991-2005 Unicode, Inc.
 * For terms of use, see http://www.unicode.org/terms_of_use.html
 * For documentation, see UAX#15.<br>
 * @author Mark Davis
 * Updates for supplementary code points:
 * Vladimir Weinstein & Markus Scherer
 */

public class Normalizer {

    /**
     * Create a normalizer for a given form.
     */
    public Normalizer(byte form, boolean fullData) {
        this.form = form;
        if (data == null) data = NormalizerBuilder.build(fullData); // load 1st time
    }

    /**
    * Masks for the form selector
    */
    static final byte
        COMPATIBILITY_MASK = 1,
        COMPOSITION_MASK = 2;

    /**
    * Normalization Form Selector
    */
    public static final byte
        D = 0 ,
        C = COMPOSITION_MASK,
        KD = COMPATIBILITY_MASK,
        KC = (byte)(COMPATIBILITY_MASK + COMPOSITION_MASK);

    /**
    * Normalizes text according to the chosen form,
    * replacing contents of the target buffer.
    * @param   source      the original text, unnormalized
    * @param   target      the resulting normalized text
    */
    public StringBuffer normalize(String source, StringBuffer target) {

        // First decompose the source into target,
        // then compose if the form requires.

        if (source.length() != 0) {
            internalDecompose(source, target);
            if ((form & COMPOSITION_MASK) != 0) {
                internalCompose(target);
            }
        }
        return target;
    }

    /**
    * Normalizes text according to the chosen form
    * @param   source      the original text, unnormalized
    * @return  target      the resulting normalized text
    */
    public String normalize(String source) {
        return normalize(source, new StringBuffer()).toString();
    }

    // ======================================
    //                  PRIVATES
    // ======================================

    /**
     * The current form.
     */
    private byte form;

    /**
    * Decomposes text, either canonical or compatibility,
    * replacing contents of the target buffer.
    * @param   form        the normalization form. If COMPATIBILITY_MASK
    *                      bit is on in this byte, then selects the recursive
    *                      compatibility decomposition, otherwise selects
    *                      the recursive canonical decomposition.
    * @param   source      the original text, unnormalized
    * @param   target      the resulting normalized text
    */
    private void internalDecompose(String source, StringBuffer target) {
        StringBuffer buffer = new StringBuffer();
        boolean canonical = (form & COMPATIBILITY_MASK) == 0;
        int ch32;
        for (int i = 0; i < source.length(); i += UTF16.getCharCount(ch32)) {
            buffer.setLength(0);
            ch32 = UTF16.charAt(source, i);
            data.getRecursiveDecomposition(canonical, ch32, buffer);

            // add all of the characters in the decomposition.
            // (may be just the original character, if there was
            // no decomposition mapping)

            int ch;
            for (int j = 0; j < buffer.length(); j += UTF16.getCharCount(ch)) {
                ch = UTF16.charAt(buffer, j);
                int chClass = data.getCanonicalClass(ch);
                int k = target.length(); // insertion point
                if (chClass != 0) {

                    // bubble-sort combining marks as necessary

                    int ch2;
                    for (; k > 0; k -= UTF16.getCharCount(ch2)) {
                        ch2 = UTF16.charAt(target, k-1);
                        if (data.getCanonicalClass(ch2) <= chClass) break;
                    }
                }
                target.insert(k, UTF16.valueOf(ch));
            }
        }
    }

    /**
    * Composes text in place. Target must already
    * have been decomposed.
    * @param   target      input: decomposed text.
    *                      output: the resulting normalized text.
    */
    private void internalCompose(StringBuffer target) {

        int starterPos = 0;
        int starterCh = UTF16.charAt(target,0);
        int compPos = UTF16.getCharCount(starterCh); // length of last composition
        int lastClass = data.getCanonicalClass(starterCh);
        if (lastClass != 0) lastClass = 256; // fix for strings staring with a combining mark
        int oldLen = target.length();

        // Loop on the decomposed characters, combining where possible

        int ch;
        for (int decompPos = compPos; decompPos < target.length(); decompPos += UTF16.getCharCount(ch)) {
            ch = UTF16.charAt(target, decompPos);
            int chClass = data.getCanonicalClass(ch);
            int composite = data.getPairwiseComposition(starterCh, ch);
            if (composite != data.NOT_COMPOSITE
            && (lastClass < chClass || lastClass == 0)) {
                UTF16.setCharAt(target, starterPos, composite);
                // we know that we will only be replacing non-supplementaries by non-supplementaries
                // so we don't have to adjust the decompPos
                starterCh = composite;
            } else {
                if (chClass == 0) {
                    starterPos = compPos;
                    starterCh  = ch;
                }
                lastClass = chClass;
                UTF16.setCharAt(target, compPos, ch);
                if (target.length() != oldLen) { // MAY HAVE TO ADJUST!
                    decompPos += target.length() - oldLen;
                    oldLen = target.length();
                }
                compPos += UTF16.getCharCount(ch);
            }
        }
        target.setLength(compPos);
    }

    /**
    * Contains normalization data from the Unicode Character Database.
    * use false for the minimal set, true for the real set.
    */
    private static NormalizerData data = null;

    /**
    * Just accessible for testing.
    */
    boolean getExcluded (char ch) {
        return data.getExcluded(ch);
    }

    /**
    * Just accessible for testing.
    */
    String getRawDecompositionMapping (char ch) {
        return data.getRawDecompositionMapping(ch);
    }
}