<?php

/**
 * An HTTP-specific implementation of a URI as defined in RFC3986. See <a href="http://www.ietf.org/rfc/rfc3986.txt">RFC 3986</a>.
 */
class HttpUri implements Serializable {
  protected $scheme=NULL;
  protected $user=NULL;
  protected $pass=NULL;
  protected $host=NULL;
  protected $port=NULL;
  protected $path=NULL;
  protected $query=NULL;
  protected $fragment=NULL;
  
  /** @brief construct a Http URI object
   *
   * Build a http uri. If other is set (another HTTP URI instance) then
   * construct this instance with the variables from the other.
   *
   * @param other defaults to null, another HTTP Uri instance
   */
  protected function __construct( $other = null )
  {
    if( $other )
    {
      $this->scheme = $other->scheme ;
      $this->user = $other->user ;
      $this->pass = $other->pass ;
      $this->host = $other->host ;
      $this->port = $other->port ;
      $this->path = $other->path ;
      $this->query = $other->query ;
      $this->fragment = $other->fragment ;
    }
  }
  
  /** @brief break the passed string apart into the different parts of a
   * url
   *
   * The parse static function takes a string and breaks it down into
   * the different parts of an HTTP Uri. It handles
   * scheme://user:password@domain:port/path?query#fragment
   *
   * @param the string to parse into an HTTP Uri
   * @returns a HTTPUri instance based on the url parts of the passed string
   */
  public static function parse( $str )
  {
    // build an empty instance to start
    $uri = new HttpUri() ;

    // now call unserialize and pass the string
    $uri->unserialize( $str ) ;

    return $uri ;
  }
  
  /** @brief resolve the string into a url
   *
   * Given the string, resolve the string into a url
   *
   * @param str string to be parsed into a url
   * @returns HTTPUri instance based on the string
   */
  public function resolve( $str )
  {
    // If the string starts with http:// then it's a HTTPUri already, so
    // return a HttpUri instance using teh static parse function
    if( 0 === strpos( $str, "http://" ) || 0 === strpos( $str, "https://" ) )
    {
      return HttpUri::parse($str);
    }

    // it's not already a url (no http:// to start the string) so create
    // an empty object and build it from the rest of the string
    $uri = new HttpUri($this);

    // If the string starts with a # character, then everything after the # is
    // a fragment
    if( 0 === strpos( $str, "#" ) )
    {
      $uri->fragment = substr( $str , 1 ) ;
      return $uri ;
    }

    // if we find a fragment pattern in the string anywhere then set it
    // in the new uri and set str to the rest of the string, else set
    // the fragment to null
    if( 1 == preg_match( "/#(.*)/", $str, $matches ) )
    {
      $uri->fragment = $matches[1];
      $str = substr( $str, 0, strlen($str) - strlen( $matches[0] ) ) ;
    }
    else
    {
      $uri->fragment = NULL ;
    }

    // if we find a question mark in the string then it's a query
    // string, set it in the uri, and set the str to what's left, else
    // set query to null
    if( 1 == preg_match( "/\\?([^#]*)/", $str, $matches ) )
    {
      $uri->query = $matches[1] ;
      $str = substr( $str, 0, strlen( $str ) - strlen( $matches[0] ) ) ;
    }
    else
    {
      $uri->query = NULL ;
    }

    // replace any backslashes with forward slashes
    $str = preg_replace("/\\/\\//","/",$str);

    // using a slah, see if there is a path in the string, break it
    // down
    $args = explode('/', $str);
    
    // Check for absolute path ($args[0] will be "" if absolute)
    if( $args[0] != "" || count( $args ) == 1 )
    {
      $tmp = explode( '/', $this->path ) ;
      array_splice( $tmp, count( $tmp ) - 1 ) ;
      $args = array_merge( $tmp, $args ) ;
    }

    // test each component of the path
    $size = count( $args ) ;
    for( $i = 0; $i < $size; $i++ )
    {
      if( $args[$i] == '.' )
      {
        array_splice( $args, $i, 1 ) ;
        $size-- ;
        $i-- ;
      }
      // we're not allowing .. to get to a parent directory
      else if( $args[$i] == '..' )
      {
        if( $i <= 1 )
	{
          trigger_error( "Invalid path using parent directory operator",
			 E_USER_NOTICE) ;
          return NULL ;
        }
        array_splice( $args, $i-1, 2 ) ;
        $size -= 2 ;
        $i -= 2 ;
      }
    }

    // set the path with what we get back from above
    $uri->path = implode('/', $args);

    // if there wasn't anything in there, set the path to /
    if(count($args)==0) $uri->path = '/';

    $matches = NULL;
    
    return $uri;
  }

  /** @brief convert the object into a string url
   *
   * Given the contents of this object, create a string representation
   * of the http url
   *
   * @return string representation of the url
   */
  public function serialize()
  {
    // start with the scheme (http:// for example)
    $str = $this->scheme."://" ;

    // if there's a user, then add it
    if( $this->user != NULL )
    {
      $str .= $this->user ;
      // if there's a password, add :password
      if( $this->pass != NULL )
      {
        $str .= ":".$this->pass;
      }
      // since we had at least a user, optionally a password, add the @
      // sign before the domain
      $str .= "@";
    }

    // add the domain
    $str .= $this->host ;

    // if we have a part, add :port
    if( $this->port != NULL )
    {
      $str .= ":".$this->port;
    }

    // if there's a path, add it. Should always be one, either just a /
    // or something more
    if( $this->path != NULL )
    {
      $str .= $this->path;

      // if there's a query string, then add ?query
      if( $this->query != NULL )
      {
        $str .= "?".$this->query ;
      }

      // if there's a fragment, add #fragment
      if( $this->fragment != NULL )
      {
        $str .= "#".$this->fragment ;
      }
    }

    return $str;
  }
  
  /** @brief convert the object to a string by calling serialize
   *
   * @return the string representation (url) of the HTTP Uri object
   */
  public function __toString()
  {
    return $this->serialize() ;
  }

  public function unserialize( $serialized )
  {
    // use the php function parse_url to break the string up into an
    // array of url parts (scheme, user, password, domain, path,
    // query, fragment
    $parts = parse_url( $serialized ) ;

    // scheme is required
    $this->scheme = $parts['scheme'] ;

    // user is optional
    if( isset( $parts['user'] ) )
    {
      $this->user = $parts['user'] ;
    }

    // password is optional.
    if( isset( $parts['pass'] ) )
    {
      $this->pass = $parts['pass'] ;
    }

    // better be a host in there. If not, then index error from php
    $this->host = $parts['host'] ;

    // port is optional
    if( isset( $parts['port'] ) )
    {
      $this->port = $parts['port'] ;
    }

    // if a path isn't specified in the URL, then slash is used
    $this->path = $parts['path'] ;

    // query is anything after ? including extra parameters &
    if( isset( $parts['query'] ) )
    {
      $this->query = $parts['query'] ;
    }

    // fragment is what is after #
    if( isset( $parts['fragment'] ) )
    {
      $this->fragment = $parts['fragment'] ;
    }
  }
}

