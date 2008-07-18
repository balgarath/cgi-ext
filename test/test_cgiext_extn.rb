require 'test/unit'
require 'cgiext'
require 'cgi'

class CGIExtTest < Test::Unit::TestCase


  def test_escape_html
  
    ## escape html characters
    input = '<>&"\''
    expected = '&lt;&gt;&amp;&quot;\''
    actual = CGIExt.escape_html(input)
    assert_equal(expected, actual)
    assert_equal(CGI.escapeHTML(input), actual)

    ## if no html characters found, return the passed argument as is
    input = 'foobar'
    expected = input
    actual = CGIExt.escape_html(input)
    assert_equal(expected, actual)
    assert_same(expected, actual)

    ## when non-string is passed, error raised
    input = nil
    assert_raise(TypeError) do
      CGIExt.escape_html(input)
    end
    input = 123
    assert_raise(TypeError) do
      CGIExt.escape_html(input)
    end
  end


  def test_escape_html!

    ## escape html characters
    input = '<>&"\''
    expected = '&lt;&gt;&amp;&quot;\''
    actual = CGIExt.escape_html!(input)
    assert_equal(expected, actual)
    assert_equal(CGI.escapeHTML(input), actual)

    ## if no html characters found, return the string as is
    input = 'foobar'
    expected = input
    actual = CGIExt.escape_html!(input)
    assert_equal(expected, actual)
    assert_same(expected, actual)

    ## non-string value are converted into string
    input = nil
    expected = ''
    actual = CGIExt.escape_html!(input)
    assert_equal(expected, actual)
    input = 123
    expected = '123'
    actual = CGIExt.escape_html!(input)
    assert_equal(expected, actual)
  end


  def test_unescape_html

    ## unescape html characters
    [
      ## html entities ('<>&"')
      ['&lt;&gt;&amp;&quot;', '<>&"'],
      ## other html entities (ex. '&copy;')
      ['&copy;&heart;', '&copy;&heart;'],
      ## '&#99' format
      ['&#34;&#38;&#39;&#60;&#62;', '"&\'<>'],
      ## '&#x9999' format
      ['&#x0022;&#x0026;&#x0027;&#x003c;&#x003E;', '"&\'<>'],
      ## invalid format
      ['&&lt;&amp&gt;&quot&abcdefghijklmn', '&<&amp>&quot&abcdefghijklmn'],
    ].each do |input, expected|
      actual = CGIExt.unescape_html(input)
      assert_equal(expected, actual)
      assert_equal(CGI.unescapeHTML(input), actual)
    end

    ## return as-is when no html entity found
    input = "foobar"
    expected = input
    actual = CGIExt.unescape_html(input)
    assert_equal(expected, actual)
    assert_same(expected, actual)

    ## when non-string is passed, error raised
    input = nil
    assert_raise(TypeError) do
      CGIExt.unescape_html(input)
    end
    input = 123
    assert_raise(TypeError) do
      CGIExt.unescape_html(input)
    end
  end


  TESTDATA_FOR_ESCAPE_URL = [
      ## example data
      ["'Stop!' said Fred._-+", '%27Stop%21%27+said+Fred._-%2B'],
      ## characters not to be escaped
      ["abcdefgxyzABCDEFGXYZ0123456789_.-",    "abcdefgxyzABCDEFGXYZ0123456789_.-"],
      ## characters to be escaped
      [' !"#$%&\'()*+,/:;<=>?@[\\]^`{|}~', "+%21%22%23%24%25%26%27%28%29%2A%2B%2C%2F%3A%3B%3C%3D%3E%3F%40%5B%5C%5D%5E%60%7B%7C%7D%7E"],
      ## '%' format
      ["k[]", 'k%5B%5D'],
      ## unicode characters
      ["\244\242\244\244\244\246\244\250\244\252", "%A4%A2%A4%A4%A4%A6%A4%A8%A4%AA"],
  ]

  def test_escape_url
    ## encode url string
    testdata = TESTDATA_FOR_ESCAPE_URL + [
                 ## containing '%' character
                 ["%XX%5%%%", "%25XX%255%25%25%25"],
               ]
    testdata.each do |input, expected|
      actual = CGIExt.escape_url(input)
      assert_equal(expected, actual)
      assert_equal(CGI.escape(input), actual)
      #assert_equal(expected, CGI.escape(input))
    end
    ## error when non-string
    assert_raise(TypeError) do
      CGIExt.escape_url(nil)
    end
    assert_raise(TypeError) do
      CGIExt.escape_url(123)
    end
  end


  def test_unescape_url
    ## decode url string
    testdata = TESTDATA_FOR_ESCAPE_URL + [
                 ## invalid '%' format
                 ["%XX%5%%%", "%XX%5%%%"],
                 ## combination of lower case and upper case
                 ["\244\252", "%a4%Aa"],
               ]
    testdata.each do |expected, input|
      actual = CGIExt.unescape_url(input)
      assert_equal(expected, actual)
      assert_equal(CGI.unescape(input), actual)
      #assert_equal(expected, CGI.unescape(input))
    end
    ## error when non-string
    assert_raise(TypeError) do
      CGIExt.unescape_url(nil)
    end
    assert_raise(TypeError) do
      CGIExt.unescape_url(123)
    end
  end


  def test_parse_query_string
    actual = nil
    ## equal with CGI.parse()
    [
      ## '&' separator
      ['a=10&b=20&key1=val1',  {'a'=>['10'], 'b'=>['20'], 'key1'=>['val1']} ],
      ## ';' separator
      ['a=10;b=20;key1=val1',  {'a'=>['10'], 'b'=>['20'], 'key1'=>['val1']} ],
      ## same keys
      ['a=1&a=2&a=3',          {'a'=>['1', '2', '3']} ],
      ## no value
      ['key=&key=', {'key'=>['', '']} ],
      ## unescape ascii string
      ['k%5B%5D=%5B%5D%26%3B', {'k[]'=>['[]&;']} ],
      ## unescape unicode string
      ["%A4%A2%a4%a4=%A4%a6%A4%A8%A4%Aa", {"\244\242\244\244"=>["\244\246\244\250\244\252"]} ],
      ## invalid '%' format
      ['k%XX%5=%%%', {'k%XX%5'=>['%%%']} ],
    ].each do |input, expected|
      actual = CGIExt.parse_query_string(input)
      assert_equal(expected, actual)
      assert_equal(CGI.parse(input), actual)
    end
    ## different with CGI.parse()
    [
      ## without '=' (CGI: {"a"=>[nil], "b"=>[nil]})
      ['a&b',     {'a'=>[''], 'b'=>['']}     ],
      ## no key  (CGI: {""=>["a", "b"], nil=>[nil]})
      ['&=a&=b',  {''=>['', 'a', 'b']}       ],
      ## invalid format  (CGI: {"a"=>[""], "b"=>[nil]})
      ['a=&b&&',  {'a'=>[''], 'b'=>[''], ''=>['', '']}  ],
    ].each do |input, expected|
      actual = CGIExt.parse_query_string(input)
      assert_equal(expected, actual)
      #assert_equal(CGI.parse(input), actual)
    end
    ## default value is frozen empty array
    assert_equal(actual['unknownkey'], [])
    assert_raise(TypeError) do
      actual['unknownkey'] << 'foo'
    end
  end


end
