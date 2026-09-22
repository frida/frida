#!/usr/bin/env python3

import sys

DOC_HEADER = '''<?xml version='1.0'?>
<?xml-stylesheet type="text/xsl" href="http://docbook.sourceforge.net/release/xsl/current/xhtml/docbook.xsl"?>
<!DOCTYPE refentry PUBLIC "-//OASIS//DTD DocBook XML V4.2//EN" "http://www.oasis-open.org/docbook/xml/4.2/docbookx.dtd">

<refentry id="{0}">
  <refmeta>
    <refentrytitle role="top_of_page" id="{0}.top_of_page">{0}</refentrytitle>
    <refmiscinfo>{0}</refmiscinfo>
  </refmeta>
  <refnamediv>
    <refname>{0}</refname>
    <refpurpose></refpurpose>
  </refnamediv>

  <refsect2 id="{1}" role="enum">
    <title>enum {1}</title>
    <indexterm zone="{1}">
      <primary>{1}</primary>
    </indexterm>
    <para><link linkend="{1}">{1}</link></para>
    <refsect3 role="enum_members">
      <title>Values</title>
      <informaltable role="enum_members_table" pgwide="1" frame="none">
        <tgroup cols="4">
          <colspec colname="enum_members_name" colwidth="300px" />
          <colspec colname="enum_members_value" colwidth="100px"/>
          <colspec colname="enum_members_description" />
          <tbody>
'''

DOC_ENUM = '''            <row role="constant">
              <entry role="enum_member_name"><para>{0}</para><para></para></entry>
              <entry role="enum_member_value"><para>= <literal>{1}</literal></para><para></para></entry>
              <entry role="enum_member_description"></entry>
            </row>'''

DOC_FOOTER = '''
          </tbody>
        </tgroup>
      </informaltable>
    </refsect3>
  </refsect2>
</refentry>
'''

if __name__ == '__main__':
    if len(sys.argv) >= 4:
        with open(sys.argv[1], 'w') as doc_out:
            enum_name = sys.argv[2]
            enum_type = sys.argv[3]

            doc_out.write(DOC_HEADER.format(enum_name, enum_type))
            for i, enum in enumerate(sys.argv[4:]):
                doc_out.write(DOC_ENUM.format(enum, i))
            doc_out.write(DOC_FOOTER)
    else:
        print('Use: ' + sys.argv[0] + ' out name type [enums]')

    sys.exit(0)
