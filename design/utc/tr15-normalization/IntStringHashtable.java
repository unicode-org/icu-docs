import java.util.Hashtable;

/**
 * Integer-String hash table. Uses Java Hashtable in stub version.
 * Copyright (c) 1991-2005 Unicode, Inc.
 * For terms of use, see http://www.unicode.org/terms_of_use.html
 * For documentation, see UAX#15.<br>
 * @author Mark Davis
 */
 
public class IntStringHashtable {
    static final String copyright = "Copyright © 1998-1999 Unicode, Inc.";
    
    public IntStringHashtable (String defaultValue) {
        this.defaultValue = defaultValue;
    }
    
    public void put(int key, String value) {
        if (value == defaultValue) {
            table.remove(new Integer(key));
        } else {
            table.put(new Integer(key), value);
        }
    }
    
    public String get(int key) {
        Object value = table.get(new Integer(key));
        if (value == null) return defaultValue;
        return (String)value;
    }
    
    private String defaultValue;
    private Hashtable table = new Hashtable();
}