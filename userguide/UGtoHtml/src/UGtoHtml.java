/*
 * Created on Sep 17, 2004
 *
  */

import java.util.zip.*;
import java.io.*;
import java.nio.channels.FileChannel;
import java.util.regex.*;
import javax.xml.parsers.*;
import org.w3c.dom.*;
import java.util.HashMap;
import java.util.Map;

/**
 * @author andy
 * 
 * Convert the ICU Userguide from Open Office to HTML format.
 * Must be executed from the user guide directory.
 * With no paramters, convert the entire userguide.
 * With parameters, convert only the specified open office (.sxw) files.
 * 
 */



public class UGtoHtml {
       
    static class Style {
        String      name        = null;
        String      parent      = null;
        boolean     bold        = false;
        boolean     italic      = false;
        boolean     strikethru  = false;
        boolean     subscript   = false;
        boolean     superscript = false;
        boolean     underline   = false;
    }
    
    
    StringBuffer  fHtml      = new StringBuffer();
    Map           fDocStyles = null;
    
    //  Function converts a single open office .sxw file to ICU User Guide OO format
    //
    void  convertFile(String fileName) {
        try {
            // Set up an InputStream on the content part of the OO .sxw (zip) file.
            String inFile = "OO/" + fileName;
            ZipFile zf = new ZipFile(inFile);
            ZipEntry ze = new ZipEntry("content.xml");
            InputStream is = zf.getInputStream(ze);

            // Set up an xml parser.
            DocumentBuilderFactory factory = DocumentBuilderFactory.newInstance();
            factory.setIgnoringComments(true);
            DocumentBuilder builder = factory.newDocumentBuilder();

            // Parse the xml content from the OO file.
            //  Need to tell the parser where the Open Office DTD is located - 
            //  The doc will reference it with a relative path in the document.
            String entityBaseURL = "file:///" + new File("OODTD").getAbsolutePath() + "/";
            Document doc = builder.parse(is, entityBaseURL);
            Element root = doc.getDocumentElement();
            
            // Build up a hash table of the auto-generated styles used in the document
            //   These are created by OO in response to applying bold, italic, etc.
            //   by hand to pieces of text, without using named styles.
            fDocStyles = getStyles(root);
            
            
            doingTableHeader = false;
            doingNote        = false;
            doingSource      = false;
                     
            // Walk the DOM tree of OO content.
            //  The actual content that we want begins just after the node
            //    of type text:sequence-decls
            Node  contentNode = root.getElementsByTagName("text:sequence-decls").item(0);
            for (;;) {
                contentNode = contentNode.getNextSibling();
                if (contentNode == null) {
                    break;
                }
                convertElement(contentNode);
            }
            
            // If the last text in the file was an icu "source" paragraph,
            //   close off the table.  TODO:  find a cleaner way to do this.
            if (doingSource) {
                // Close off a icu-source table box.
                fHtml.append("</pre></td></tr></table>\n");
                doingSource = false;
            }

            
            //
            // Merge the generated html with the template file contents
            String html = addTemplateFile("html-template/ugtemplate.html");
            
            //
            // Write the new html file
            String outFileName   = fileName.replaceFirst("(.*)\\.sxw", "html/$1.html");
            OutputStream       f = new FileOutputStream(outFileName);
            OutputStreamWriter o = new OutputStreamWriter(f, "utf8");
            o.write(html);
            o.close();

        }
        catch (Exception e) {
            System.out.println("Exception processing file " + fileName + ": " + e + "\n");
        }
    };

    
    //  convertElement      Convert an Open Office node, and, recursively, any children.
    //
    static boolean doingTableHeader = false;
    static boolean doingNote        = false;
    static boolean doingSource      = false;
    void convertElement(Node n) {
    	boolean	processChildNodes = true;
        String  xmlTag    = "";
        String  styleAttr = "";
        
        if (n.getNodeType() == Node.ELEMENT_NODE) {
            Element e = (Element)n;
            xmlTag    = e.getTagName();
            styleAttr = e.getAttribute("text:style-name");
            
            // If the style associated with this element is a local generated
            //  paragraph style, replace it with the base style that it was derived from.
            //  This occurs if paragraphs have attributes like "keep with next paragraph",
            //   but are otherwise based on an ICU style.
            Style st = (Style)fDocStyles.get(styleAttr);
            if (st != null && st.parent!=null) {
            	styleAttr = st.parent;
            }
            
            // icu-source formatted paragraphs are collected into a table in the HTML.
            //  This check is for any construct in the OO doc that would close off
            //  an in-progress icu-source section.
            if (doingSource && !(
                    xmlTag.equals("text:p") && styleAttr.equals("icu-source") ||
                    xmlTag.equals("text:s") ||
                    xmlTag.equals("text:line-break")
                    ) ) {
                // Close off a icu-source table box.
                fHtml.append("</pre></td></tr></table>\n");
                doingSource = false;
            }
            
            if (xmlTag.equals("text:p") && styleAttr.equals("icu-note")) {
                if (doingNote == false) {
                fHtml.append("<table border=\"0\" cellpadding=\"0\" cellspacing=\"3\">\n" +
                        "<tr><td valign=\"top\" width=\"20\">\n" + 
                        "<img alt=\"Note\" border=\"0\" height=\"24\" hspace=\"0\" " +
                        "src=\"images/note.gif\" vspace=\"0\" width=\"20\"/>" +
                        "</td><td valign=\"top\"><i>");
                } else {
                    fHtml.append("<table border=\"0\" cellpadding=\"0\" cellspacing=\"3\">\n" +
                            "<tr><td valign=\"top\" width=\"20\">\n" +                            
                            "</td><td valign=\"top\"><i>");                    
                }
                doingNote = true;
            } else if (xmlTag.equals("text:p") && styleAttr.equals("icu-Text body")) {
                fHtml.append("<p>");
                doingNote = false;
            } else if (xmlTag.equals("text:line-break")) {
                fHtml.append("<br/>");
            } else if (xmlTag.equals("text:s")) {
                int numSpaces = getIntAttribute(e, "text:c");
                for (int i=0; i<numSpaces; i++) {
                    fHtml.append("&nbsp;");                  
                }
            } else if (xmlTag.equals("text:h") && styleAttr.equals("icu-Heading 1")) {
                fHtml.append("<h1>");
            } else if (xmlTag.equals("text:h") && styleAttr.equals("icu-Heading 2")) {
                fHtml.append("<h2>");
            } else if (xmlTag.equals("text:h") && styleAttr.equals("icu-Heading 3")) {
                fHtml.append("<h3>");
            } else if (xmlTag.equals("text:h") && styleAttr.equals("icu-Heading 4")) {
                fHtml.append("<h4>");
                
            } else if (xmlTag.equals("text:p") && styleAttr.equals("icu-Heading 1")) {
                fHtml.append("<h1>");
            } else if (xmlTag.equals("text:p") && styleAttr.equals("icu-Heading 2")) {
                fHtml.append("<h2>");
            } else if (xmlTag.equals("text:p") && styleAttr.equals("icu-Heading 3")) {
                fHtml.append("<h3>");
            } else if (xmlTag.equals("text:p") && styleAttr.equals("icu-Heading 4")) {
                fHtml.append("<h4>");
                
            } else if (xmlTag.equals("text:span") && styleAttr.equals("icu-code")) {
                fHtml.append("<FONT face=\"courier, monospaced\">");
            } else if (xmlTag.equals("text:span") && fDocStyles.get(styleAttr)!=null) {
                doStyleOpenTags(styleAttr);
                 
            } else if (xmlTag.equals("text:unordered-list")) {
                fHtml.append("<ul>");
            } else if (xmlTag.equals("text:ordered-list")) {
                fHtml.append("<ol>");
            } else if (xmlTag.equals("text:list-item")) {
                fHtml.append("<li>");
                
            } else if (xmlTag.equals("text:tab-stop")) {
                fHtml.append("\t");
            } else if (xmlTag.equals("table:table")) {
                fHtml.append("<table>");
            } else if (xmlTag.equals("text:p") && styleAttr.equals("Table Contents")) {
                // No action required.
            } else if (xmlTag.equals("text:p") && styleAttr.equals("icu-source")) {
                if (doingSource == false) {
                    doingSource = true;
                    fHtml.append("<table border=\"1\" cellpadding=\"8\" cellspacing=\"0\"><tr>" +
                                 "<td bgcolor=\"#EEEEEE\" valign=\"top\">\n<pre>");
                    
                }
            } else if (xmlTag.equals("table:table-column")) {
                // No action required.
            } else if (xmlTag.equals("table:table-header-rows")) {
                doingTableHeader = true;
            } else if (xmlTag.equals("table:table-row")) {
                fHtml.append("<tr bgcolor=\"#99ccff\">");
            } else if (xmlTag.equals("table:table-cell")) {
                fHtml.append(doingTableHeader? "<th bgcolor=\"#cccccc\"" : "<td bgcolor=\"#EEEEEE\"");
                String colspan = e.getAttribute("table:number-columns-spanned");
                if (!(colspan.equals("") && colspan.equals("1"))) {
                    fHtml.append(" colspan=\"" + colspan + "\"");
                }
                fHtml.append(">");
            } else if (xmlTag.equals("table:covered-table-cell")) {
                // empty elements of this type appears after a cell that covers multiple columns.
                // No action is needed.   Ignore it.
            } else if (xmlTag.equals("text:p") && styleAttr.equals("Table Heading")) {
                // no action needed.  Table cell contents.
            } else if (xmlTag.equals("text:p") && styleAttr.matches("P\\d+")) {
                // No action needed, local "Pnn" style.
                //   TODO:  Some paragraphs may have styles bold, italic, etc. styles.
             } else if (xmlTag.equals("text:a") && !e.getAttribute("xlink:href").equals("")) {
                 String target = e.getAttribute("xlink:href");
                 // The target of a link may be to somewhere in another Open Office doc that we're
                 //   converting.  Switch the .sxw to link to the html.
                 String fixedTarget = target.replaceFirst("\\.sxw(?:(?=#)|$)", ".html");
                 fixedTarget = fixedTarget.replaceAll("&", "&amp;");
                 fHtml.append("<a href=\"" + fixedTarget + "\">"); 
                 if (fixedTarget.equals("#")) {
                 	System.out.println("Bad href of \"#\"");
                 }
             } else if (xmlTag.equals("text:initial-creator")) {
             	// No action required.
            } else if (xmlTag.equals("text:bookmark")) {
             	String target = e.getAttribute("text:name");
             	fHtml.append("<a name=\"" + target + "\"></a>");
            } else if (xmlTag.equals("text:bookmark-start")) {
                // Note: OO bookmarks covering a range do not need to nest with evenly within
                //      other stuff.  To avoid generating bad html, just anchor to the start position,
                //      and ignore the bookmark-end tag.
             	String target = e.getAttribute("text:name");
             	fHtml.append("<a name=\"" + target + "\"></a>");
            } else if (xmlTag.equals("text:bookmark-end")) {
                // no action.
            } else if (xmlTag.equals("office:annotation")) {
            	// This is an Open Office "note".
            	//  It may be a reference to an image.
            	//  It may be some other note that the doc author inserted.
            	String noteText = getChildContent(n);
            	String imageName = noteText.replaceFirst("\\s*?html image name:", "");
            	if (imageName.equals(noteText)==false) {
            		// the regular expression matched, causing the replaceFirst to do something,
            		//   which causes the before and after strings to be different.
            		//   All of which means the note contains the name of a user guide image file.
            		imageName = imageName.trim();
            		fHtml.append("<img src=\"images/" + imageName + "\" alt=\"TODO:\"/>\n");
                    
                    // Copy the image file itself into the html/images directory
                    copyFile("OO/images/"+imageName, "html/images/"+imageName);
            	} else {
                    System.out.println("skipping note: " + noteText);   
                }
            	// Whether or not the note was a reference to an image name,
            	//   we don't want the text content of the note to appear in the
            	//   html version of the user guide.
            	processChildNodes = false;
            } else if (xmlTag.startsWith("draw:")) {
            	processChildNodes = false;
             	// ignore embedded images.
            } else {
                System.out.print("Unhandled <" + xmlTag);
                if (xmlTag.startsWith("text")) {
                    System.out.print(" text:style-name=\"" + styleAttr + "\"");                       
                }
                System.out.println(" ... >");
            }
        }
            
        
        
        if (n.getNodeType() == Node.TEXT_NODE) {
            // Text node.  Most of the time we just dump the content straight into the
            //   output html.
            // This includes various white space & new lines that may exist in the
            //   OO xml when its "pretty print" save option is enabled.  In general,
            //   this will also pretty-print our html.
            // One exception, if we are "doingSource" we need to discard any text
            //   that isn't the child of a text node.
            try {
            if (doingSource) {
                Element parent = (Element)n.getParentNode();
                String tagName = parent.getTagName();
                if (!tagName.equals("text:p")) {
                    throw new Exception();
                }
            }
            String text = n.getNodeValue();
            String escapedText = text.replaceAll("&", "&amp;");
            escapedText = escapedText.replaceAll("<", "&lt;");
            fHtml.append(escapedText);
            }
            catch (Exception e) {              
            }
        }
        
        // Recusively do any children of this node.
        if (processChildNodes) {
        	for (Node child=n.getFirstChild(); child!=null; child=child.getNextSibling()) {
        		convertElement(child);
        	}
        }
        
        // Do the close tags, if needed.
        if (n.getNodeType() == Node.ELEMENT_NODE) {
            Element e = (Element)n;
            if (xmlTag.equals("text:p") && styleAttr.equals("icu-Text body")) {
                fHtml.append("</p>");
            } else if (xmlTag.equals("text:h") && styleAttr.equals("icu-Heading 1")) {
                fHtml.append("</h1>");
            } else if (xmlTag.equals("text:h") && styleAttr.equals("icu-Heading 2")) {
                fHtml.append("</h2><p/>");
            } else if (xmlTag.equals("text:h") && styleAttr.equals("icu-Heading 3")) {
                fHtml.append("</h3><p/>");
            } else if (xmlTag.equals("text:h") && styleAttr.equals("icu-Heading 4")) {
                fHtml.append("</h4><p/>");
            } else if (xmlTag.equals("text:p") && styleAttr.equals("icu-Heading 1")) {
                fHtml.append("</h1>");
            } else if (xmlTag.equals("text:p") && styleAttr.equals("icu-Heading 2")) {
                fHtml.append("</h2><p/>");
            } else if (xmlTag.equals("text:p") && styleAttr.equals("icu-Heading 3")) {
                fHtml.append("</h3><p/>");
            } else if (xmlTag.equals("text:p") && styleAttr.equals("icu-Heading 4")) {
                fHtml.append("</h4><p/>");
            } else if (xmlTag.equals("text:p") && styleAttr.equals("icu-source")) {
                // Sometimes Open Office puts a LF at the end of icu-source paragraphs, sometimes not.
                //  The html needs one in all cases, but not two.
                fHtml.append("\n");
            } else if (xmlTag.equals("text:span") && styleAttr.equals("icu-code")) {
                fHtml.append("</FONT>");
            } else if (xmlTag.equals("text:span") && fDocStyles.get(styleAttr)!=null) {
                doStyleCloseTags(styleAttr);
            } else if (xmlTag.equals("text:unordered-list")) {
                fHtml.append("</ul>\n");
            } else if (xmlTag.equals("text:ordered-list")) {
                fHtml.append("</ol>");
            } else if (xmlTag.equals("text:list-item")) {
                fHtml.append("</li>\n");
            } else if (xmlTag.equals("text:p") && styleAttr.equals("icu-note")) {
                fHtml.append("</i></td></tr></table>\n");
            } else if (xmlTag.equals("table:table")) {
                fHtml.append("</table>");
            } else if (xmlTag.equals("table:table-header-rows")) {
                doingTableHeader = false;
            } else if (xmlTag.equals("table:table-row")) {
                fHtml.append("</tr>");
            } else if (xmlTag.equals("table:table-cell")) {
                fHtml.append(doingTableHeader? "</th>" : "</td>");
            } else if (xmlTag.equals("text:a") && !e.getAttribute("xlink:href").equals("")) {
                fHtml.append("</a>\n");                
            } 

         }
                    
    }

    //  Get an integer valued XML attribute value from an elment node
    //
    int getIntAttribute(Element e, String attrName) {
        String att    = e.getAttribute(attrName);
        int    retVal = 0;
        if (att != null) {
            retVal = Integer.parseInt(att);
        }
        return retVal;
    }
    
    // Get the text content appearing under a node in the document.
    // General purpose utility function.
    String getChildContent(Node n) {
    	String s = "";
    	Node   child;
    	switch (n.getNodeType()) {
    	case Node.TEXT_NODE:
    		s = n.getNodeValue();
    	break;
    	case Node.ELEMENT_NODE:
    		for (child = n.getFirstChild(); child!=null; child=child.getNextSibling()) {
    			s += getChildContent(child);
    		}
    	break;
    	default:
    		s = "";
    	}
    	return s;
    }
    
    
    // Get the OpenOffice Automatic styles for the document
    //  Put them into a hash table, indexed by name.
    //  These style names will show up on OO text spans and paragraphs, and we
    //   will need to refer back to the style definitions here to know what is needed
    //   in the generated html.
    //  Example of an OO style definition:
    //  <style:style style:name="T1" style:family="text">
    //       <style:properties fo:font-weight="bold" style:font-weight-asian="bold" 
    //                     style:font-weight-complex="bold"/>
    //  </style:style>

    Map  getStyles(Element doc) {
        HashMap m = new HashMap(100);
        NodeList nl = doc.getElementsByTagName("office:automatic-styles");
        if (nl.getLength() == 0) {
            return m;
        }
        Element automatic_Styles = (Element)nl.item(0);
        Element styleE;
        for (Node n =automatic_Styles.getFirstChild(); n!=null; n=n.getNextSibling()) {
            if (n.getNodeType() != Node.ELEMENT_NODE) {
                continue;
            }
            styleE = (Element)n;
            String styleName = styleE.getAttribute("style:name");
            String family  = styleE.getAttribute("style:family");
            if (!(family.equals("text") || family.equals("paragraph"))) {
                continue;
            }
            Style style = new Style();
            style.name = styleE.getAttribute("style:name");
            m.put(style.name, style);
            
            // If this is a paragraph style, get the style that it was based on.
            if (family.equals("paragraph")) {
            	style.parent = styleE.getAttribute("style:parent-style-name");	
            }
            
            // Find the "style:properties" child element
            Element styleProps =  null;
            for (Node p=styleE.getFirstChild(); p!=null; p=p.getNextSibling()) {
                if (p.getNodeName().equals("style:properties") ) {
                    styleProps = (Element)p;
                    break;
                }
            }
            
            if (styleProps == null) {
            	// Some P (paragraph) styles don't have a style:properies element.
            	//  Just skip over those.
                continue;
            }
            
            if (styleProps.getAttribute("fo:font-weight").equals("bold")) {
                style.bold = true;
            }
            if (!(styleProps.getAttribute("style:text-underline").equals("") ||
                    styleProps.getAttribute("style:text-underline").equals("none"))) {
                style.underline = true;
            }
            if (styleProps.getAttribute("fo:font-style").equals("italic")) {
                style.italic = true;
            }
            if (!(styleProps.getAttribute("style:text-crossing-out").equals("")  ||
            	styleProps.getAttribute("style:text-crossing-out").equals("none"))) {
                style.strikethru = true;
            }
            if (styleProps.getAttribute("style:text-position").startsWith("sub ")) {
                style.subscript = true;
            }
            if (styleProps.getAttribute("style:text-position").startsWith("super")) {
                style.superscript = true;
            }
        }
        return m;
    }
    
    void doStyleOpenTags(String styleName) {
        Style s = (Style)fDocStyles.get(styleName);
        if (s.bold) {
            fHtml.append("<b>");
        }
        if (s.italic) {
            fHtml.append("<i>");
        }
        if (s.strikethru) {
            fHtml.append("<s>");
        }
        if (s.subscript) {
            fHtml.append("<sub>");
        }
        if (s.superscript) {
            fHtml.append("<sup>");
        }
        if (s.underline) {
            fHtml.append("<u>");
        }
    }
    
    void doStyleCloseTags(String styleName) {
        Style s = (Style)fDocStyles.get(styleName);
        if (s.underline) {
            fHtml.append("</u>");
        }
        if (s.superscript) {
            fHtml.append("</sup>");
        }
        if (s.subscript) {
            fHtml.append("</sub>");
        }
        if (s.strikethru) {
            fHtml.append("</s>");
        }
        if (s.italic) {
            fHtml.append("</i>");
        }
        if (s.bold) {
            fHtml.append("</b>");
        }
    }
    
    
    
    //
    //  Copy a file
    //
    static void copyFile(String source, String destination) {
        try {
            FileChannel src = new FileInputStream(source).getChannel();
            FileChannel dest = new FileOutputStream(destination).getChannel();
            dest.transferFrom(src, 0, src.size());
            dest.close();
            src.close();            
        }
        catch (IOException e) {
            System.out.println("copy " + source + " to " + destination + "  Error: " + e);
            throw new RuntimeException(e);
        }
    }
      
    
    //  Read the template file, substitute in the generated html,
    //  return the combination in a string.
    String addTemplateFile(String name) {
        try {
            StringBuffer       template = new StringBuffer();
            FileInputStream    f  = new FileInputStream(name);
            InputStreamReader  in = new InputStreamReader(f, "utf8");
            int                c;
            for (;;) {
                c = in.read();
                if (c < 0) {
                    break;     
                }
                template.append((char)c);
            }
            in.close();
            
            // Locate the region where the html is to be inserted.
            String  startMark = "<!-- HTML conversion tool insert content HERE -->";
            String  endMark   = "<!-- END of generated html content -->";
            int startIndex = template.indexOf(startMark) + startMark.length();
            int endIndex   = template.indexOf(endMark);
            
            // Substitute in the newly generated html
            template.replace(startIndex, endIndex, fHtml.toString());
            return template.toString();
        }
        catch (IOException e) {
            System.out.println("Error: e");
            throw new RuntimeException(e);
        }
    }
    
    
    //
    //  main - Either convert the files listed on the command line,
    //          or read the Open Office master document for the user guide, 
    //          suck out a list of include .sxw files, and convert them all
    //
    public static void main(String[] args) {
        //
        //  Create the output directories "html" and "html/images"
        //    if they don't already exist.
        //
        File htmlDir = new File("html");
        if (htmlDir.exists() == false) {
            htmlDir.mkdir();
        }
        if (htmlDir.isDirectory() == false) {
            System.err.println("Error - ./html/ can't be created.");
            System.exit(-1);
        }
        File imagesDir = new File("html/images");
        if (imagesDir.exists() == false) {
            imagesDir.mkdir();
        }
        if (imagesDir.isDirectory() == false) {
            System.err.println("Error - ./html/images/ can't be created.");
            System.exit(-1);
        }
       
    
        //
        //  Copy css and image files that are unconditionally required.
        //
        copyFile("html-template/icu.css", "html/icu.css");
        copyFile("html-template/images/w24.gif", "html/images/w24.gif");    // The ICU Unicode World Logo
        copyFile("OO/images/note.gif", "html/images/note.gif");  // The little yellow "note" image.
        copyFile("html-template/images/ibm_logo.gif", "html/images/ibm_logo.gif");
        copyFile("html-template/images/gr100.gif", "html/images/gr100.gif");
        copyFile("html-template/images/c.gif", "html/images/c.gif");

        if (args.length > 0) {
            for (int i=0; i<args.length; i++) {
                System.out.println("converting file "+ args[i]);
                UGtoHtml This = new UGtoHtml();
                This.convertFile(args[i]);
            }
        } else {
            
            System.out.println("Opening OO/userguide.sxg\n");
            ZipFile zipFile = null;
            try {
                zipFile = new ZipFile("OO/userguide.sxg");
                ZipEntry zipEntry = new ZipEntry("content.xml");
                InputStream is = zipFile.getInputStream(zipEntry);
                
                // Set up an xml parser.
                DocumentBuilderFactory factory = DocumentBuilderFactory.newInstance();
                factory.setIgnoringComments(true);
                DocumentBuilder builder = factory.newDocumentBuilder();

                // Parse the xml content from the OO file.
                //  Need to tell the parser where the Open Office DTD is located - 
                //  The doc will reference it with a relative path in the document.
                String entityBaseURL = "file:///" + new File("OODTD").getAbsolutePath() + "/";
                Document doc = builder.parse(is, entityBaseURL);

                NodeList inclList = doc.getElementsByTagName("text:section-source");
                int  numSections = inclList.getLength();
                for (int n=0; n<numSections; n++) {
                    Element textSection = (Element)inclList.item(n);
                    String chapterFile = textSection.getAttribute("xlink:href");
                    System.out.println(chapterFile);
                    UGtoHtml This = new UGtoHtml();
                    This.convertFile(chapterFile);
                }
                
            }
            catch (Exception e) {
                System.out.println("Exception: " + e);
            }
        }
    }
}
