<?xml version="1.0"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
  <xsl:output method="xml" encoding="ISO-8859-1" indent="yes"
      doctype-public="-//W3C//DTD XHTML 1.0 Transitional//EN"
      doctype-system="http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd"/>

  <xsl:variable name="href_base" select="''"/>
  <xsl:variable name="menu_name">Main Menu</xsl:variable>
<!--
 - returns the filename associated to an ID in the original file
 -->
  <xsl:template name="filename">
    <xsl:param name="name" select="string(@href)"/>
    <xsl:choose>
      <xsl:when test="$name = '#Overview'">
        <xsl:text>overview.html</xsl:text>
      </xsl:when>
      <xsl:when test="$name = '#Config'">
        <xsl:text>config.html</xsl:text>
      </xsl:when>
      <xsl:when test="$name = '#News'">
        <xsl:text>news.html</xsl:text>
      </xsl:when>
      <xsl:when test="$name = '#Downloads'">
        <xsl:text>downloads.html</xsl:text>
      </xsl:when>
      <xsl:when test="$name = '#Developers'">
        <xsl:text>devel.html</xsl:text>
      </xsl:when>
      <xsl:when test="$name = '#Internals'">
        <xsl:text>internals.html</xsl:text>
      </xsl:when>
      <xsl:when test="$name = '#Contacts'">
        <xsl:text>contacts.html</xsl:text>
      </xsl:when>
      <xsl:when test="$name = '#FAQ'">
        <xsl:text>FAQ.html</xsl:text>
      </xsl:when>
      <xsl:when test="$name = '#Debug'">
        <xsl:text>debug.html</xsl:text>
      </xsl:when>
      <xsl:when test="$name = '#Using'">
        <xsl:text>using.html</xsl:text>
      </xsl:when>
      <xsl:when test="$name = '#Python'">
        <xsl:text>python.html</xsl:text>
      </xsl:when>
      <xsl:when test="$name = '#Security'">
        <xsl:text>security.html</xsl:text>
      </xsl:when>
      <xsl:when test="$name = '#Differences'">
        <xsl:text>differences.html</xsl:text>
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="$name"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>
<!--
 - The global title
 -->
  <xsl:variable name="globaltitle" select="string(/html/body/h1[1])"/>
<!--
 - The table of content
 -->
  <xsl:variable name="toc">
    <ul><!-- style="margin-left: -1em" -->
      <li><a href="index.html">Home</a></li>
      <xsl:for-each select="/html/body/h2">
        <xsl:variable name="filename">
          <xsl:call-template name="filename">
            <xsl:with-param name="name" select="concat('#', string(a[1]/@name))"/>
          </xsl:call-template>
        </xsl:variable>
	<xsl:if test="$filename != ''">
	  <li>
	    <xsl:element name="a">
	      <xsl:attribute name="href">
	        <xsl:value-of select="$filename"/>
	      </xsl:attribute>
	      <xsl:if test="$filename = 'docs.html'">
		  <xsl:attribute name="style">font-weight:bold</xsl:attribute>
	      </xsl:if>
	      <xsl:value-of select="."/>
	    </xsl:element>
	  </li>
	</xsl:if>
      </xsl:for-each>
      <li><a href="ChangeLog.html">ChangeLog</a></li>
    </ul>
  </xsl:variable>
  <xsl:variable name="related">
    <ul>
      <li><a href="http://mail.gnome.org/archives/gamin-list/">Mail archive</a></li>
      <li><a href="http://oss.sgi.com/projects/fam/">FAM project</a></li>
       <li><a href="sources/">sources</a></li>
       <li><a href="http://bugzilla.gnome.org/buglist.cgi?product=gamin&amp;bug_status=UNCONFIRMED&amp;bug_status=NEW&amp;bug_status=ASSIGNED&amp;bug_status=NEEDINFO&amp;bug_status=REOPENED&amp;bug_status=RESOLVED&amp;bug_status=VERIFIED&amp;form_name=query">GNOME Bugzilla</a></li>
       <li><a href="https://bugzilla.redhat.com/bugzilla/buglist.cgi?product=Fedora+Core&amp;product=Red+Hat+Enterprise+Linux&amp;component=fam&amp;component=gamin&amp;bug_status=NEW&amp;bug_status=ASSIGNED&amp;bug_status=REOPENED&amp;bug_status=MODIFIED&amp;short_desc_type=allwordssubstr&amp;short_desc=&amp;long_desc_type=allwordssubstr&amp;long_desc=&amp;Search=Search">Red Hat Bugzilla</a></li>
    </ul>
  </xsl:variable>
  <xsl:template name="toc">
    <table border="0" cellspacing="0" cellpadding="1" width="100%" bgcolor="#000000">
      <tr>
        <td>
          <table width="100%" border="0" cellspacing="1" cellpadding="3">
            <tr>
              <td colspan="1" bgcolor="#eecfa1" align="center">
                <center>
                  <b><xsl:value-of select="$menu_name"/></b>
                </center>
              </td>
            </tr>
            <tr>
              <td bgcolor="#fffacd">
                <xsl:copy-of select="$toc"/>
              </td>
            </tr>
          </table>
          <table width="100%" border="0" cellspacing="1" cellpadding="3">
            <tr>
              <td colspan="1" bgcolor="#eecfa1" align="center">
                <center>
                  <b>Related links</b>
                </center>
              </td>
            </tr>
            <tr>
              <td bgcolor="#fffacd">
                <xsl:copy-of select="$related"/>
              </td>
            </tr>
          </table>
        </td>
      </tr>
    </table>
  </xsl:template>
  <xsl:template mode="head" match="title">
    <title>
      <xsl:apply-templates/>
    </title>
  </xsl:template>
  <xsl:template mode="head" match="meta">
</xsl:template>
<!--
 - Write the styles in the head
 -->
  <xsl:template name="style">
    <!--
    <link rel="SHORTCUT ICON" href="/favicon.ico"/> 
      -->
    <style type="text/css">
TD {font-family: Verdana,Arial,Helvetica}
BODY {font-family: Verdana,Arial,Helvetica; margin-top: 2em; margin-left: 0em; margin-right: 0em}
H1 {font-family: Verdana,Arial,Helvetica}
H2 {font-family: Verdana,Arial,Helvetica}
H3 {font-family: Verdana,Arial,Helvetica}
A:link, A:visited, A:active { text-decoration: underline }
</style>
  </xsl:template>
<!--
 - Write the title box on top
 -->
  <xsl:template name="titlebox">
    <xsl:param name="title" select="'Main Page'"/>
    <table border="0" width="100%" cellpadding="5" cellspacing="0" align="center">
    <tr>
    <td width="120">
    </td>
    <td>
    <table border="0" width="90%" cellpadding="2" cellspacing="0" align="center" bgcolor="#000000">
      <tr>
        <td>
          <table width="100%" border="0" cellspacing="1" cellpadding="3" bgcolor="#fffacd">
            <tr>
              <td align="center">
                <xsl:element name="h1">
                  <xsl:value-of select="$globaltitle"/>
                </xsl:element>
                <xsl:element name="h2">
                  <xsl:value-of select="$title"/>
                </xsl:element>
              </td>
            </tr>
          </table>
        </td>
      </tr>
    </table>
    </td>
    </tr>
    </table>
  </xsl:template>
<!--
 - Handling of nodes in the body before the first H2, table of content
 - Everything is just copied over, except href which may get rewritten
 - and h1/h2/a at the top level
 -->
  <xsl:template priority="2" mode="subcontent" match="a">
    <xsl:variable name="filename">
      <xsl:call-template name="filename">
        <xsl:with-param name="name" select="string(@href)"/>
      </xsl:call-template>
    </xsl:variable>
    <xsl:copy>
      <xsl:attribute name="href">
        <xsl:value-of select="$filename"/>
      </xsl:attribute>
      <xsl:apply-templates mode="subcontent" select="node()"/>
    </xsl:copy>
  </xsl:template>
  <xsl:template mode="subcontent" match="@*|node()">
    <xsl:copy>
      <xsl:apply-templates mode="subcontent" select="@*|node()"/>
    </xsl:copy>
  </xsl:template>
  <xsl:template mode="content" match="@*|node()">
    <xsl:if test="name() != 'h1' and name() != 'h2'">
      <xsl:copy>
        <xsl:apply-templates mode="subcontent" select="@*|node()"/>
      </xsl:copy>
    </xsl:if>
  </xsl:template>
<!--
 - Handling of nodes in the body after an H2
 - Open a new file and dump all the siblings up to the next H2
 -->
  <xsl:template name="subfile">
    <xsl:param name="header" select="following-sibling::h2[1]"/>
    <xsl:variable name="filename">
      <xsl:call-template name="filename">
        <xsl:with-param name="name" select="concat('#', string($header/a[1]/@name))"/>
      </xsl:call-template>
    </xsl:variable>
    <xsl:variable name="tocfilename">
      <xsl:call-template name="filename">
        <xsl:with-param name="name" select="concat('#', string($header/a[1]/@name))"/>
      </xsl:call-template>
    </xsl:variable>
    <xsl:variable name="title">
      <xsl:value-of select="$header"/>
    </xsl:variable>
    <xsl:document href="{$filename}" method="xml" encoding="ISO-8859-1"
      doctype-public="-//W3C//DTD XHTML 1.0 Transitional//EN"
      doctype-system="http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
      <html>
        <head>
          <xsl:call-template name="style"/>
          <xsl:element name="title">
            <xsl:value-of select="$title"/>
          </xsl:element>
        </head>
        <body bgcolor="#8b7765" text="#000000" link="#a06060" vlink="#000000">
          <xsl:call-template name="titlebox">
            <xsl:with-param name="title" select="$title"/>
          </xsl:call-template>
          <table border="0" cellpadding="4" cellspacing="0" width="100%" align="center">
            <tr>
              <td bgcolor="#8b7765">
                <table border="0" cellspacing="0" cellpadding="2" width="100%">
                  <tr>
                    <td valign="top" width="200" bgcolor="#8b7765">
		      <xsl:choose>
		        <xsl:when test="$filename = 'docs.html'">
                          <xsl:call-template name="develtoc"/>
		        </xsl:when>
		        <xsl:when test="$tocfilename = ''">
                          <xsl:call-template name="develtoc"/>
		        </xsl:when>
		        <xsl:otherwise>
                          <xsl:call-template name="toc"/>
		        </xsl:otherwise>
		      </xsl:choose>
                    </td>
                    <td valign="top" bgcolor="#8b7765">
                      <table border="0" cellspacing="0" cellpadding="1" width="100%">
                        <tr>
                          <td>
                            <table border="0" cellspacing="0" cellpadding="1" width="100%" bgcolor="#000000">
                              <tr>
                                <td>
                                  <table border="0" cellpadding="3" cellspacing="1" width="100%">
                                    <tr>
                                      <td bgcolor="#fffacd">
                                        <xsl:apply-templates mode="subfile" select="$header/following-sibling::*[preceding-sibling::h2[1] = $header         and name() != 'h2' ]"/>
					<p><a href="contacts.html">Daniel Veillard</a></p>
                                      </td>
                                    </tr>
                                  </table>
                                </td>
                              </tr>
                            </table>
                          </td>
                        </tr>
                      </table>
                    </td>
                  </tr>
                </table>
              </td>
            </tr>
          </table>
        </body>
      </html>
    </xsl:document>
  </xsl:template>
  <xsl:template mode="subfile" match="@*|node()">
    <xsl:copy>
      <xsl:apply-templates mode="content" select="@*|node()"/>
    </xsl:copy>
  </xsl:template>
<!--
 - Handling of the initial body and head HTML document
 -->
  <xsl:template match="body">
    <xsl:variable name="firsth2" select="./h2[1]"/>
    <xsl:variable name="rest2" select="./h2[position()&gt;1]"/>
    <body bgcolor="#8b7765" text="#000000" link="#a06060" vlink="#000000">
      <xsl:call-template name="titlebox">
        <xsl:with-param name="title" select="'Gamin project page'"/>
      </xsl:call-template>
      <table border="0" cellpadding="4" cellspacing="0" width="100%" align="center">
        <tr>
          <td bgcolor="#8b7765">
            <table border="0" cellspacing="0" cellpadding="2" width="100%">
              <tr>
                <td valign="top" width="200" bgcolor="#8b7765">
                  <xsl:call-template name="toc"/>
                </td>
                <td valign="top" bgcolor="#8b7765">
                  <table border="0" cellspacing="0" cellpadding="1" width="100%">
                    <tr>
                      <td>
                        <table border="0" cellspacing="0" cellpadding="1" width="100%" bgcolor="#000000">
                          <tr>
                            <td>
                              <table border="0" cellpadding="3" cellspacing="1" width="100%">
                                <tr>
                                  <td bgcolor="#fffacd">
                                    <xsl:apply-templates mode="content" select="($firsth2/preceding-sibling::*)"/>
                                    <xsl:for-each select="./h2">
                                      <xsl:call-template name="subfile">
                                        <xsl:with-param name="header" select="."/>
                                      </xsl:call-template>
                                    </xsl:for-each>
				    <p><a href="contacts.html">Daniel Veillard</a></p>
                                  </td>
                                </tr>
                              </table>
                            </td>
                          </tr>
                        </table>
                      </td>
                    </tr>
                  </table>
                </td>
              </tr>
            </table>
          </td>
        </tr>
      </table>
    </body>
  </xsl:template>
  <xsl:template match="head">
    <head>
      <xsl:call-template name="style"/>
      <xsl:apply-templates mode="head"/>
    </head>
  </xsl:template>
  <xsl:template match="html">
    <xsl:message>Generating the Web pages</xsl:message>
    <html>
      <xsl:apply-templates/>
    </html>
  </xsl:template>
</xsl:stylesheet>
