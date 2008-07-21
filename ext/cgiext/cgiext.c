/*
cgiext.c

This is a c extension of some of cgi.rb's parsing functions

Some code borrowed from Makoto Kuwata, http://www.kuwata-lab.com

*/

#include <stdio.h>
#include <ruby.h>
#include <assert.h>

VALUE cCGIExt;
static ID _id_to_s;


//number of characters between 2 indices in a string
#define LEN(start, end)  ((size_t)(end-start))/sizeof(char)

VALUE cgiext_parse_query_string(VALUE self, VALUE string);
static VALUE next_token(char *s, char *p);


/**
 *   from cgi.rb:
 *
 *   # Escape special characters in HTML, namely &\"<>
 *   #   CGI::escapeHTML('Usage: foo "bar" <baz>')
 *   #      # => "Usage: foo &quot;bar&quot; &lt;baz&gt;"
 *   def CGI::escapeHTML(string)
 *     string.gsub(/&/n, '&amp;').gsub(/\"/n, '&quot;').gsub(/>/n, '&gt;').gsub(/</n, '&lt;')
 *   end
 */
VALUE cgiext_escape_html(VALUE self, VALUE value) {
    Check_Type(value, T_STRING);
    char *s, *p;
    s = p = RSTRING(value)->ptr;
    int ch;
    while ((ch = *p) != '\0') {
        if (ch == '&' || ch == '<' || ch == '>' || ch == '"')
            break;
        ++p;
    }
    if (ch == '\0')
        return value;
    VALUE str = next_token(s, p);
    s = p;
    while ((ch = *p) != '\0') {
        switch (ch) {
        case '&':
            rb_str_cat(str, s, LEN(s, p));
            s = p + 1;
            rb_str_cat(str, "&amp;", 5);
            break;
        case '<':
            rb_str_cat(str, s, LEN(s, p));
            s = p + 1;
            rb_str_cat(str, "&lt;", 4);
            break;
        case '>':
            rb_str_cat(str, s, LEN(s, p));
            s = p + 1;
            rb_str_cat(str, "&gt;", 4);
            break;
        case '"':
            rb_str_cat(str, s, LEN(s, p));
            s = p + 1;
            rb_str_cat(str, "&quot;", 6);
            break;
        default:
            NULL;
        }
        ++p;
    }
    rb_str_cat(str, s, LEN(s, p));
    return str;
}


VALUE cgiext_escape_html_(VALUE self, VALUE value) {
    VALUE str;
#if 0
    str = rb_String(value);
#else
    switch (TYPE(value)) {
    case T_STRING:
        str = value;
        break;
    case T_NIL:
        return rb_str_new("", 0);
    case T_FIXNUM:
    case T_FLOAT:
    case T_BIGNUM:
        /*return rb_String(value);*/
        return rb_funcall(value, _id_to_s, 0);
    default:
        /*str = rb_String(value);*/
        str = rb_funcall(value, _id_to_s, 0);
    }
#endif
    return cgiext_escape_html(self, str);
}


#define _ishexchar(ch) (isdigit(ch) || ('a'<=ch && ch<='f') || ('A'<=ch && ch<='F'))

static int _hexchar2int(int ch);
static int _isunicode(void);

/**
 *   from cgi.rb:
 *
 *   # Unescape a string that has been HTML-escaped
 *   #   CGI::unescapeHTML("Usage: foo &quot;bar&quot; &lt;baz&gt;")
 *   #      # => "Usage: foo \"bar\" <baz>"
 *   def CGI::unescapeHTML(string)
 *     string.gsub(/&(amp|quot|gt|lt|\#[0-9]+|\#x[0-9A-Fa-f]+);/n) do
 *       match = $1.dup
 *       case match
 *       when 'amp'                 then '&'
 *       when 'quot'                then '"'
 *       when 'gt'                  then '>'
 *       when 'lt'                  then '<'
 *       when /\A#0*(\d+)\z/n       then
 *         if Integer($1) < 256
 *           Integer($1).chr
 *         else
 *           if Integer($1) < 65536 and ($KCODE[0] == ?u or $KCODE[0] == ?U)
 *             [Integer($1)].pack("U")
 *           else
 *             "&##{$1};"
 *           end
 *         end
 *       when /\A#x([0-9a-f]+)\z/ni then
 *         if $1.hex < 256
 *           $1.hex.chr
 *         else
 *           if $1.hex < 65536 and ($KCODE[0] == ?u or $KCODE[0] == ?U)
 *             [$1.hex].pack("U")
 *           else
 *             "&#x#{$1};"
 *           end
 *         end
 *       else
 *         "&#{match};"
 *       end
 *     end
 *   end
 */
VALUE cgiext_unescape_html(VALUE self, VALUE string) {
    Check_Type(string, T_STRING);
    char *s, *p;
    s = p = RSTRING(string)->ptr;
    int ch;
    while ((ch = *p) != '\0' && ch != '&') {
        ++p;
    }
    if (ch == '\0')
        return string;
    VALUE str = next_token(s, p);
    s = p;
    const int size = 8;
    char buf[size];
    char append[2];
    int is_unicode = -1;
    while ((ch = *p) != '\0') {
        if (ch == '&') {
            char *pbuf = buf;
            char *limit = p + size;
            char *p_amp = p;
            while ((ch = *++p) != '\0' && p < limit && ch != ';' && ch != '&') {
                *pbuf++ = ch;
            }
            if (ch == '\0')
                break;
            if (p == limit || ch == '&')
                continue;
            assert(ch == ';');
            *pbuf = '\0';
            append[0] = append[1] = '\0';
            if (buf[0] == '#') {
                int ch2 = -1;
                int val = 0;
                char *pch = buf + 1;
                if (*pch == 'x' || *pch == 'X') {
                    int v;
                    while (*++pch != '\0' && (v = _hexchar2int(*pch)) >= 0)
                        val = (val << 4) | v;
                }
                else {
                    while (*pch != '\0' && isdigit(*pch))
                        pch++;
                    if (*pch == '\0')
                        val = strtol(buf+1, NULL, 10);
                }
                if (*pch == '\0') {
                    if (val < 256) {
                        append[0] = val;
                    }
                    else if (val < 65536) {
                        if (is_unicode < 0) {
                            int kcode = rb_get_kcode()[0];
                            is_unicode = kcode == 'U' || kcode == 'u';
                        }
                        if (is_unicode) {
                            append[0] = (val & 0xF0) >> 4;
                            append[1] = val & 0x0F;
                        }
                    }
                }
            }
            else {
                if      (strcmp(buf, "amp")  == 0)  append[0] = '&';
                else if (strcmp(buf, "lt")   == 0)  append[0] = '<';
                else if (strcmp(buf, "gt")   == 0)  append[0] = '>';
                else if (strcmp(buf, "quot") == 0)  append[0] = '"';
            }
            if (append[0] != '\0') {
                rb_str_cat(str, s, LEN(s, p_amp));
                s = p + 1;
                rb_str_cat(str, append, append[1] == '\0' ? 1 : 2);
            }
        }
        ++p;
    }
    rb_str_cat(str, s, LEN(s, p));
    return str;
}


/**
 *   from cgi.rb:
 *
 *   # URL-encode a string.
 *   #   url_encoded_string = CGI::escape("'Stop!' said Fred")
 *   #      # => "%27Stop%21%27+said+Fred"
 *   def CGI::escape(string)
 *     string.gsub(/([^ a-zA-Z0-9_.-]+)/n) do
 *       '%' + $1.unpack('H2' * $1.size).join('%').upcase
 *     end.tr(' ', '+')
 *   end
 */
VALUE cgiext_escape_url(VALUE self, VALUE string)
{
    Check_Type(string, T_STRING);
    char *s, *p;
    s = p = RSTRING(string)->ptr;
    int ch;
    while ((ch = *p) != '\0') {
        if (!isalnum(ch) && ch != '_' && ch != '.' && ch != '-')
            break;
        if (ch == ' ')
            break;
        ++p;
    }
    if (ch == '\0')
        return string;
    VALUE str = next_token(s, p);
    s = p;
    char buf[4];
    buf[0] = '%';
    while ((ch = *p) != '\0') {
        if (isalnum(ch) || ch == '_' || ch == '.' || ch == '-') {
            ++p;
        }
        else if (ch == ' ') {
            if (s < p)
                rb_str_cat(str, s, LEN(s, p));
            s = ++p;
            rb_str_cat(str, "+", 1);
        }
        else {
            if (s < p)
                rb_str_cat(str, s, LEN(s, p));
            s = ++p;
            /*sprintf(buf, "%x", ch+1);*/
            int val1 = (ch & 0xF0) >> 4;
            int val2 = ch & 0x0F;
            buf[1] = val1 + (val1 <= 9 ? '0' : ('A' - 10));
            buf[2] = val2 + (val2 <= 9 ? '0' : ('A' - 10));
            rb_str_cat(str, buf, 3);
        }
    }
    if (s < p)
        rb_str_cat(str, s, LEN(s, p));
    return str;
}


static int _hexchar2int(int ch) {
    if (isdigit(ch))
        return ch - '0';
    if ('A' <= ch && ch <= 'F')
        return ch - 'A' + 10;
    if ('a' <= ch && ch <= 'f')
        return ch - 'a' + 10;
    return -1;
}


/**
 *   from cgi.rb:
 *
 *   # URL-decode a string.
 *   #   string = CGI::unescape("%27Stop%21%27+said+Fred")
 *   #      # => "'Stop!' said Fred"
 *   def CGI::unescape(string)
 *     string.tr('+', ' ').gsub(/((?:%[0-9a-fA-F]{2})+)/n) do
 *       [$1.delete('%')].pack('H*')
 *     end
 *   end
 */
VALUE cgiext_unescape_url(VALUE self, VALUE string)
{
    Check_Type(string, T_STRING);
    char *s, *p;
    s = p = RSTRING(string)->ptr;
    char ch;
    while ((ch = *p) != '\0') {
        if (ch == '%' || ch == '+')
            break;
        ++p;
    }
    if (ch == '\0')
        return string;
    VALUE str = next_token(s, p);
    s = p;
    while ((ch = *p) != '\0') {
        if (ch == '%') {
            int hex, val;
            if ((ch = *++p) == '\0')
                break;
            if ((hex = _hexchar2int(ch)) < 0) {
                ++p;
                continue;
            }
            val = hex << 4;
            if ((ch = *++p) == '\0')
                break;
            if ((hex = _hexchar2int(ch)) < 0) {
                ++p;
                continue;
            }
            val = val | hex;
            rb_str_cat(str, s, LEN(s, p-2));
            s = ++p;
            char buf[1];
            buf[0] = val;
            rb_str_cat(str, buf, 1);
        }
        else if (ch == '+') {
            rb_str_cat(str, s, LEN(s, p));
            s = ++p;
            rb_str_cat(str, " ", 1);
        }
        else {
            ++p;
        }
    }
    rb_str_cat(str, s, LEN(s, p));
    return str;
}


/**
 *   from cgi.rb:
 *
 *   # Parse an HTTP query string into a hash of key=>value pairs.
 *   #
 *   #   params = CGI::parse("query_string")
 *   #     # {"name1" => ["value1", "value2", ...],
 *   #     #  "name2" => ["value1", "value2", ...], ... }
 *   #
 *   def CGI::parse(query)
 *     params = Hash.new([].freeze)
 * 
 *     query.split(/[&;]/n).each do |pairs|
 *       key, value = pairs.split('=',2).collect{|v| CGI::unescape(v) }
 *       if params.has_key?(key)
 *         params[key].push(value)
 *       else
 *         params[key] = [value]
 *       end
 *     end
 * 
 *     params
 *   end
 */
static VALUE _frozen_empty_arr;
static ID _id_setdefault;
static ID _id_freeze;

VALUE cgiext_parse_query_string(VALUE self, VALUE string) {
    //make sure we are dealing with a string
    Check_Type(string, T_STRING);
    
    //params is a hash of arrays that will be returned
    VALUE params = rb_hash_new();
    
    VALUE key = Qnil, val = Qnil;
    
    //using 2 pointers, s and p, to keep track of current string    
    char *s, *p;
    s = p = RSTRING(string)->ptr;

    //ch will be the current character we are checking
    int ch = *p;
    
    //infinite loop, will only break when end of query string is reached
    while (1) {
        
        //reached an equal sign, store the value between s and p as key
        if (ch == '=' && key == Qnil) 
        {
            key = next_token(s, p);
            s = p + 1;
        }
        
        //reached the end of a key:value pair, or the end of the query string
        else if (ch == '&' || ch == ';' || ch == '\0') 
        {
            
            //if key has been stored already, store the string between s and p as its val
            if (key != Qnil) 
            {
                val = next_token(s, p);
            }
            
            //else store the string between s and p as the key, with an empty val
            else 
            {
                key = next_token(s, p);
                val = rb_str_new("", 0);
            }
            
            key = cgiext_unescape_url(self, key);
            val = cgiext_unescape_url(self, val);

            // set parameter
            VALUE list = rb_hash_aref(params, key);              // list = params[key]

            if (list == Qnil)
                rb_hash_aset(params, key, rb_ary_new3(1, val));  // params[key] = [val]

            else
                rb_ary_push(list, val);                          // list << val

            // break if end of query string
            if (ch == '\0')
                break;

        //set everything for next iteration
            key = val = Qnil;
            s = p + 1;
        }
        ch = *++p;
    }
    
    VALUE empty_arr = rb_funcall(rb_ary_new(), _id_freeze, 0);
    rb_funcall(params, _id_setdefault, 1, empty_arr);    // params.default = [].freeze
    return params;
}


//private function used in several functions
//returns the current token between s and p

static VALUE next_token(char *s, char *p)
{
  return rb_str_new(s, LEN(s, p));
} 

static VALUE _get_module(char *modname, char *submodname) {
    VALUE module;
    if (! rb_const_defined(rb_cObject, rb_intern(modname)))
        return Qnil;
    module = rb_const_get(rb_cObject, rb_intern(modname));
    if (submodname != NULL)
        module = rb_const_get(module, rb_intern(submodname));
    return module;
}


static VALUE cgiext_replace_functions(VALUE self) {
    VALUE module;
    if ((module = _get_module("CGI", NULL)) != Qnil) {
        rb_define_module_function(module, "parse",        cgiext_parse_query_string, 1);
        rb_define_module_function(module, "escapeHTML",   cgiext_escape_html, 1);
        rb_define_module_function(module, "unescapeHTML", cgiext_unescape_html, 1);
        rb_define_module_function(module, "escape",       cgiext_escape_url, 1);
        rb_define_module_function(module, "unescape",     cgiext_unescape_url, 1);
    }

    return Qnil;
}


void Init_cgiext(void) {
    cCGIExt = rb_define_module("CGIExt");
    _id_to_s   = rb_intern("to_s");
    _id_setdefault = rb_intern("default=");
    _id_freeze = rb_intern("freeze");
    _frozen_empty_arr = rb_funcall(rb_ary_new(), rb_intern("freeze"), 0);
    rb_define_module_function(cCGIExt, "parse_query_string", cgiext_parse_query_string, 1);
    rb_define_module_function(cCGIExt, "escape_html", cgiext_escape_html, 1);
    rb_define_module_function(cCGIExt, "escape_html!", cgiext_escape_html_, 1);
    rb_define_module_function(cCGIExt, "unescape_html", cgiext_unescape_html, 1);
    rb_define_module_function(cCGIExt, "escape_url", cgiext_escape_url, 1);
    rb_define_module_function(cCGIExt, "unescape_url", cgiext_unescape_url, 1);
    rb_define_singleton_method(cCGIExt, "replace_functions", cgiext_replace_functions, 0);
    cgiext_replace_functions(cCGIExt);
}
