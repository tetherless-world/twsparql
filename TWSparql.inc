<?php
// $Id$

include_once 'HttpUri.inc';

/**
 * Defines a logger object for TW Sparql module to provide feedback to users.
 * @author pattoe 
 */
interface TWSparqlLogger {
  
  /**
   * Return whether debugging is turned on or not
   */
  public function shouldDebug();

  /**
   * Set whether or not we should debug
   */
  public function enableDebug($val);

  /**
   * Logs a string as an error.
   * @param string $str String to log to the error console. 
   */
  public function logError($str);
  
  /**
   * Logs a string as a warning
   * @param string $str String to log to the warning console. 
   */
  public function logWarning($str);
  
  /**
   * Logs a string as a status
   * @param string $str String to log to the status console. 
   */
  public function logStatus($str);
}

/**
 * Defines a cache object for the TWSparql module to cache intermediate and 
 * final results of a query.
 * @author pattoe
 */
interface TWSparqlCache {

  /**
   * Caches a query document for a particular URI
   * @param string $pageid An id representing the page being cached
   * @param string $queryUri A URI that 'executes' the query file to get
   * the query text
   * @param string $endpoint The endpoint URL the query is run against
   * @param string $results The query text returned by dereferencing $queryUri 
   */
  public function cacheQueryResult($pageid, $queryUri, $endpoint, $results);
  
  /**
   * Caches the final transformation of a query using an XSL template.
   * @param string $pageid An id representing the page being cached
   * @param string $queryUri The URI used to generate the SPARQL results
   * @param string $endpoint The endpoint URL the query is run against
   * @param string $xsltUri The URI used to dereference the XSL stylesheet
   * @param string $result The final result applying the transformation named by
   * $xsltUri to the document returned by deferencing $queryUri.
   */
  public function cacheFinal($pageid, $queryUri, $endpoint, $xsltUri, $result);
  
  /**
   * Clears the cached copy of $queryUri, if any.
   * @param string $pageid An id representing the page being cached
   * @param string $queryUri 
   * @param string $endpoint The endpoint URL the query is run against
   * @param string $xsltUri The URI used to dereference the XSL stylesheet
   */
  public function clearQueryResultCache($pageid, $queryUri, $endpoint);
  public function clearFinalCache($pageid, $queryUri, $endpoint, $xsltUri);
  
  /**
   * Gets the cached content for $queryUri, or NULL if no content is available.
   * @param string $pageid An id representing the page being cached
   * @param string $queryUri 
   * @param string $endpoint The endpoint URL the query is run against
   * @param string $xsltUri The URI used to dereference the XSL stylesheet
   * @param string $expires The string representation of the datetime
   * the cached item expires
   */
  public function getCachedQueryResult($pageid, $queryUri, $endpoint, $expires);
  public function getCachedFinal($pageid, $queryUri, $endpoint, $xsltUri, $expires);
}

abstract class TWSparql {
  
  public static $engine = NULL;
  public static $cache = NULL;
  public static $logger = NULL;
  protected $params = NULL ;
  protected $curTag = NULL ;
  protected $curQueryUri = NULL;
  protected $curQueryText = NULL;
  protected $curEndpoint = NULL;
  protected $curXsltUri = NULL;
  protected $curXsltDoc = NULL;
  protected $curRDFDoc = NULL;
  protected $curReferer = NULL;
  protected $pageid = NULL;
  protected $sendAccept = TRUE;

  /**
   * Sets whether to send the Accept: header to the remote endpoint. This
   * is a hack added to prevent an NPE in the VIVO endpoint.
   * @param boolean $accept If TRUE, the engine will send an Accept header
   * for application/rdf+xml or text/turtle to the endpoint. If FALSE, no
   * Accept header will be sent.
   */
  public function setSendAccept( $accept )
  {
    $this->sendAccept = $accept == TRUE; // force to TRUE/FALSE from bool-like
  }

  /**
   * Gets whether the engine will send the HTTP Accept header when querying
   * a remote endpoint.
   * @returns TRUE if an Accept header will be sent, otherwise FALSE.
   */
  public function getSendAccept()
  {
    return $this->sendAccept;
  }

  /**
   * Gets a static instance of the caching mechanism for the TW Sparql engine
   * @return TWSparqlCache
   */
  public static function getCache() {
    return TWSparql::$cache;
  }
  
  /**
   * Gets a static instance of the logger used for communicating information
   * to the containing CMS.
   * @return TWSparqlLogger
   */
  public static function getLogger() {
    return TWSparql::$logger;
  }
  
  /**
   * Gets a static instance of the TW SPARQL Engine for the CMS.
   * @return TWSparql
   */
  public static function getEngine() {
    return TWSparql::$engine;
  }
  
  /** Main entry point for the TWSparql interface
   *
   * This is the main function, main entry point for the common TWSparql
   * interface. This is the function that controls the flow of operation
   * for rendering the content.
   *
   * The content of the page to be rendered is passed to this function.
   * In that content could be sparql tags, one or more. For each tag:
   *
   * - parse the tag, pulling out any and all attributes specified
   * - execute the query (query could be cached, or use query from
   * previous tag)
   * - transform the results (xslt could be cached, results could be
   * cached)
   * - Replace the sparql tag in the content with the rendered content
   *
   * @param int $pageid The identifier for this page
   * @param string $content The body of the page in the CMS to be
   * rendered
   */
  public function render( $pageid, $content )
  {
	TWSparql::getLogger()->logStatus( "*****STARTING A NEW QUERY*****." ) ;

	$this->pageid = $pageid ;

    $newContent = "" ;
  
    // Match script tag
    $matches = array() ;
  
	// search for sparql tags
    if( 0 < preg_match_all( "/(<p>)?<sparql(([^\/>]*(\/[^>])?)*)(\/>|>(([^<]*(<[^\/])?)*)<\/sparql>)(<\/p>|<br \/>)?/",$content,$matches,PREG_OFFSET_CAPTURE ) )
	{
	  // parse the attributes of the sparql tag. Keep track of how long
	  // this takes
      $start = microtime( TRUE ) ;
      $this->params = TWSparql::parseParams( $matches ) ;
      $end = microtime( TRUE ) ;
	  TWSparql::getLogger()->logStatus( "Processing SPARQL parameters took ".(($end-$start)*1000)." milliseconds." ) ;

      $start = 0;
    
	  // The query file, query results, xslt file, and rendered content
	  // could be cached. But we call a generic function to execute a
	  // query, transform the results.

	  // can be multiple sparql tags, for each one ...
      foreach( $this->params as $desc )
	  {
		$this->curTag = $desc ;

		// save off the current uri
		if( $desc["uri"] && $desc["uri"] != "" )
		{
			$this->curReferer = $desc["uri"] ;
		}

		// add everything from the end of the last sparql tag up to
		// this sparql tag
        $newContent .= substr( $content, $start, $desc["startChar"]-$start ) ;

		// go get the results of the query/transform and add it to the
		// content
        $newContent .= $this->getSparqlResult() ;

		// new start is the end of this tag
        $start = $desc["endChar"];
      }
    
      $content = $newContent . substr($content,$start);
	  $content .= "<script type=\"text/javascript\">window.twsparql = \"".$this->getSiteBase()."twsparql/query\";</script>";
    }
  
	// - replace the tag with the rendered result
    return preg_replace("/<\/sparql>(<\/p>)?/","",$content);
  }

  /**
   * Parses each &lt;sparql&gt; tag and their attributes
   * @param array $matches Output from preg_match_all
   * @return 
   */
  public static function parseParams($matches)
  {
    $params = array();

    $tagText = $matches[0];
    $paramText = $matches[2];
    $queryText = $matches[6];

    for($i=0;$i<count($tagText);$i++) {
      $text = $tagText[$i];
      $desc = array();
      $desc["startChar"] = $text[1];
      $desc["length"] = strlen($text[0]);
      $desc["endChar"] = $desc["startChar"] + $desc["length"];
      $desc["endpoint"] = $desc["query"] = $desc["uri"] = $desc["i"] = null;
      $desc["s"] = $desc["form"] = $desc["query-text"] = $desc["xslt"] = null;
      $desc["offset"] = $desc["limit"] = null;
      $desc["other"] = array() ;
      $pairs = array();
      $count = preg_match_all("/([[:alpha:]]*)=\"([^\"]*)\"/",
          $paramText[$i][0], $pairs);
      for($j=0;$j<$count;$j++) {
        switch($pairs[1][$j]) {
        case "endpoint":
          $desc["endpoint"] = $pairs[2][$j];
		  if( strtolower( $desc["endpoint"] ) == "default" )
		  {
			$desc["endpoint"] = $this->getEndpoint() ;
		  }
		  break;
        case "query":
          $desc["query"] = $pairs[2][$j]; break;
        case "uri":
          $desc["uri"] = $pairs[2][$j]; break;
        case "i":
          $desc["uri"] = TWSparql::rfc2396(TWSparql::getEngine()->getIbase(),$pairs[2][$j]);
          break;
        case "s":
          $desc["uri"] = TWSparql::rfc2396(TWSparql::getEngine()->getSbase(),$pairs[2][$j]);
          break;
        case "form":
          $desc["form"] = $pairs[2][$j]; break;
        case "xslt":
          $desc["xslt"] = $pairs[2][$j]; break;
        case "expire":
          $desc["expire"] = $pairs[2][$j]; break;
        case "offset":
          $desc["offset"] = $pairs[2][$j]; break;
        case "limit":
          $desc["limit"] = $pairs[2][$j]; break;
	case "params":
	  $desc["params"] = $pairs[2][$j]; break;
        default:
          $desc["other"][$pairs[1][$j]] = $pairs[2][$j];
        }
      }
      if(!isset($desc["uri"]) || $desc["uri"] == "") {
        if(isset($_GET["uri"])) {
          $desc["uri"] = $_GET["uri"];
        }
      }
      if( isset( $desc["params"] ) && isset( $_GET ) )
      {
	$oparams = explode( ",", $desc["params"] ) ;
	foreach( $oparams as $gkey )
	{
	    if( $_GET[$gkey] )
	    {
		$desc["other"][$gkey] = $_GET[$gkey] ;
	    }
	}
      }
      if(!isset($queryText[$i])) { }
      else if(!is_array($queryText[$i]) && strlen($queryText[$i])>0) {
        $desc["query-text"] = $queryText[$i];
      }
      else if(is_array($queryText[$i]) && strlen($queryText[$i][0])>0) {
        $desc["query-text"] = $queryText[$i][0];
      }

      if( !isset( $desc["expire"] ) )
      {
        $desc["expire"] = TWSparql::getEngine()->getCacheTTL() ;
      }

      $params[] = $desc;
    }

    return $params;
  }
  
  /** For each sparql tag we get the query text, execute the sparql
   * query, and transform the results.
   *
   * @return string content to replace the sparql tag
   */
  public function getSparqlResult()
  {
	// dump out the attributes of the current tag
	if( TWSparql::getLogger()->shouldDebug() )
	{
		$first = true ;
		$msg = "tag attributes: " ;
		foreach( $this->curTag as $var => $val ) {
			if( !$first ) $msg .= ", " ;
			$msg .= "$var = $val" ;
			$first = false ;
		}
		TWSparql::getLogger()->logStatus( $msg ) ;
		$this->dump() ;
	}

	// if no query is specified in the sparql tag, and no query has yet
	// been done for this page, then return an error (not logged, but
	// becomes part of the page)
    if( $this->curTag["query"] == null
	    && $this->curTag["query-text"] == null
		&& $this->curQueryUri == null
		&& $this->curQueryText == null )
	{
      return "<p>TW SPARQL Error: No query specified</p>" ;
	}

	$twError = null ;

	// get the query URI for this sparql tag. The query URI is more than
	// just the query parameter in the tag. We take the query attribute
	// from the tag and attach the base query path (if it doesn't start
	// with http already), and then add a query string if a uri is
	// specified, a limit, an offset, or any of the other parameters.
	$this->getQueryUri( $twError ) ;
	if( $twError != null )
	{
	  return $twError ;
	}
	TWSparql::getLogger()->logStatus( "query uri: " .  $this->curTag["queryUri"] ) ;

	// generate the XSLT URI. Could just be a file name, so prepend the
	// base URL, and append the referer if we're using a uri attribute
	// in the sparql tag.
	$this->getXsltUri( $twError ) ;
	if( $twError != null )
	{
      return $twError ;
    }
	TWSparql::getLogger()->logStatus( "xslt uri: " .  $this->curTag["xsltUri"] ) ;

	// generate the Endpoint URI. This should just be the endpoint URL
	// without the query
	$this->getEndpointUri( $twError ) ;
	if( $twError != null )
	{
		return $twError ;
	}
	TWSparql::getLogger()->logStatus( "endpoint uri: " .  $this->curTag["endpoint"] ) ;

    // see if the resulting xhtml is already cached
    $xhtml = TWSparql::$cache->getCachedFinal(
                                    $this->pageid, $this->curTag["queryUri"],
                                    $this->curTag["endpoint"],
                                    $this->curTag["xsltUri"],
                                    $this->curTag["expire"]) ;
    if( $xhtml != NULL )
    {
      $this->curQueryUri = $this->curTag["queryUri"] ;
      $this->curEndpoint = $this->curTag["endpoint"] ;
      $this->curXsltUri = $this->curTag["xsltUri"] ;
	  $this->curXsltDoc = null ;

      return $xhtml ;
    }

    // final result wasn't cached, but maybe the query result is cached
    $queryResult = TWSparql::$cache->getCachedQueryResult(
                                                $this->pageid,
                                                $this->curTag["queryUri"],
                                                $this->curTag["endpoint"],
                                                $this->curTag["expire"]) ;
    if( $queryResult != NULL )
    {
      // Given the RDF/XML results, create the new RDF document
      $rdf = new DOMDocument();
      $rdf->substituteEntities = TRUE;
      if( !$rdf->loadXML( $queryResult ) )
      {
        $queryError = "<p>TW SPARQL Error: SPARQL Endpoint did not return a valid XML document</p>" ;
        return ;
      }

      $this->curRDFDoc = $rdf;
    }
    else
    {
      // we know the query URI now, go get the query text. This text could
      // already be saved, if the query is the same as the last sparql
      // tag, or could be cached.
      $this->getQueryText( $twError ) ;
      if( $twError != null )
      {
        return $twError ;
      }
      TWSparql::getLogger()->logStatus( "query text: " . htmlentities( $this->curQueryText ) ) ;

      // execute the query given the query text we just got. The result
      // could already be generated or could be cached.
      $start = microtime(TRUE);
      $this->executeQuery( $twError ) ;
      if( $twError != null )
      {
        return $twError ;
      }
      $end = microtime(TRUE);
      TWSparql::getLogger()->logStatus( "SPARQL Query took ".(($end-$start)*1000)." milliseconds." ) ;
    }

	$this->curQueryUri = $this->curTag["queryUri"] ;
    $this->curEndpoint = $this->curTag["endpoint"] ;

	// with the xslt uri, we can go grab the text of the xslt file and
	// create the xslt document from it.
	$this->getXsltDoc( $twError ) ;
	if( $twError != null )
	{
		return $twError ;
	}
	TWSparql::getLogger()->logStatus( "xslt document retrieved" ) ;

	// given the rdf document from the query execution, and the xslt
	// document, we can now transform the results.
    $start = microtime(TRUE);
	$xhtml = $this->transform( $twError ) ;
	if( $twError != null )
	{
		return $twError ;
	}
    $end = microtime(TRUE);
	TWSparql::getLogger()->logStatus( "XSL Transformation took ".(($end-$start)*1000)." milliseconds." ) ;

    TWSparql::$cache->cacheFinal( $this->pageid,
                                  $this->curTag["queryUri"],
                                  $this->curTag["endpoint"],
                                  $this->curTag["xsltUri"],
                                  $xhtml ) ;

	// @todo There was more done here. Probably need to look into that.
	return $xhtml ;
  }
  
  /** get the query uri for the current sparql tag
   *
   * If one is not specified in the sparql tag then we use the last
   * query uri from the last sparql tag. The query uri is the base URL
   * (if the query attribute value doesn't start with http), the name of
   * the query file, and any parameters that we need to pass to the php
   * in order to generate the query text.
   *
   * The resulting query uri is stored in $this->curTag["uri"]
   *
   * @param string $queryError passed by reference, store any error
   * messages in this string
   */
  public function getQueryUri( &$queryError )
  {
	if( $this->curTag["query"] != "" )
	{
      $path = $this->curTag["query"] ;
      if( 0 !== strpos( $path, "http://" ) )
	  {
        $path = TWSparql::rfc2396($this->getQueryPath(),$path);
      }

      if( $this->curTag["uri"] )
	  {
        $path .= (strpos($path,"?")===FALSE ? "?" : "&")."uri=".urlencode($this->curTag["uri"]);
      }

      if($this->curTag["limit"]!=null)
	  {
        $path .= (strpos($path,"?")===FALSE ? "?" : "&")."limit=".$this->curTag["limit"];
	  }

      if($this->curTag["offset"]!=null)
	  {
        $path .= (strpos($path,"?")===FALSE ? "?" : "&")."offset=".$this->curTag["offset"];
	  }
      if( $this->curTag["other"] != null )
      {
        foreach( $this->curTag["other"] as $okey => $oval )
        {
          $path .= (strpos($path,"?")===FALSE ? "?" : "&")."$okey=".$oval;
        }
      }
	  $this->curTag["queryUri"] = $path ;
	}
	else
	{
	  // a query attribute is not specified in the tag, so use the last
	  // one
	  $this->curTag["queryUri"] = $this->curQueryUri ;
	}
  }

  /** generate the XSLT URI for the current sparql tag
   *
   * The XSLT URI includes the name of the XSLT file to use, or a full
   * path to an XSLT document on the web, and if a uri is specified, use
   * it as the referer
   *
   * The resulting XSLT URI is stored in $this->curTag["xsltUri"]
   *
   * @param string $xsltError passed by reference, store any error
   * messages in this string
   */
  public function getXsltUri( &$xsltError )
  {
	if( $this->curTag["xslt"] != "" )
	{
		$path = $this->curTag["xslt"] ;
		if( 0 !== strpos( $path, "http://" ) )
		{
		  $path = TWSparql::rfc2396($this->getXsltPath(),$path);
		}
		/*
		if( $this->curReferer )
		{
		  $path .= "?r=" . urlencode( $this->curReferer ) ;
		}
		*/

		$this->curTag["xsltUri"] = $path ;
	}
	else
	{
		$this->curTag["xsltUri"] = $this->curXsltUri ;
	}
  }

  /** generate the Endpoint URI for the current sparql tag
  *
  * The Endpoint URI is simply the endpoint without the query string.
  * Might include the return format information
  *
  * The resulting Endpoint is stored in $this->curTag["endpoint"]
  * 
  * @param string $xsltError passed by reference, store any error
  * messages in this string
  */
  public function getEndpointUri( &$twError )
  {
    if( empty( $this->curTag["endpoint"] ) )
    {
      if( $this->curEndpoint != "" )
      {
        $this->curTag["endpoint"] = $this->curEndpoint ;
      }
      else
      {
        $this->curTag["endpoint"] = $this->curEndpoint = $this->getEndpoint() ;
      }
    }
  }

  /** retrieve the query text to be used to query the endpoint
   *
   * The query text could already exist from a previous sparql tag, or
   * could be cached, or we go get the query text from the query file.
   *
   * The resulting query text is stored in $this->curQueryText
   *
   * @param string $queryError passed by reference, store any error
   * messages in this string
   */
  public function getQueryText( &$queryError )
  {
	// if a query was already run on this page, and it's the same as
	// this current tag, and we already have the query text, then return
	// it
	if( ( $this->curTag["queryUri"] == $this->curQueryUri ) && $this->curQueryText != NULL )
	{
		TWSparql::getLogger()->logStatus( "ALREADY HAVE THE QUERY TEXT" ) ;
		return ;
	}

	// didn't already exist, clear the current versions
	$this->curQueryText = NULL ;
	$this->curRDFDoc = NULL ;

    $query = @file_get_contents( $this->curTag["queryUri"] ) ;
    if( !$query || $query == null || $query == "" )
    {
	  $queryError = "<p>TW SPARQL Error: 404 Query not found</p>" ;
	  return ;
    }

	// save the current query text for future reference
	$this->curQueryText = $query ;

	return ;
  }

  /**
   * Performs the work of executing a SPARQL query against an endpoint via HTTP.
   * @param string $endpoint HTTP-dereferenceable SPARQL endpoint URL
   * @param string $queryText A valid SPARQL query
   * @param string $queryError Reference to a variable where an error message will
   * be stored if the query fails.
   * @returns string The result of the query, if any, as a string.
   */
  protected function executeQuery_( $endpoint, $queryText, &$queryError )
  {
    $endpoint .= strrpos( $endpoint, "?" ) !== FALSE ? "&" : "?";
    $endpoint .= "query=".urlencode( $queryText ) ;
    $headers = $this->sendAccept ? "Accept: application/sparql-results+json, ".
      "application/sparql-results+xml, application/rdf+xml" : NULL ;
    $httpOpts = array("method" => "GET");
    if ( $headers )
    {
      $httpOpts["header"] = $headers;
    }
    $ctx = stream_context_create( array( "http" => $httpOpts ) );
    TWSparql::getLogger()->logStatus( "Executing query: $endpoint" ) ;
    $result = @file_get_contents($endpoint,false,$ctx);
    if( $result == FALSE )
    {
      $queryError = "<p>TW SPARQL Error: SPARQL Endpoint did not return an XML document</p>" ;
      return NULL;
    }
    TWSparql::getLogger()->logStatus( "query returned: $result" ) ;
    return $result;
  }

  /**
   * Helper function for executing a query against an endpoint. This method is
   * public to assist developers building modules on top of twsparql.
   * @param string $endpoint HTTP-dereferenceable SPARQL endpoint URL
   * @param string $queryText A valid SPARQL query
   * @param string $queryError Reference to a variable where an error message will
   * be stored if the query fails.
   * @returns DOMDocument The result of the query, if any, as a DOMDocument.
   */
  public function executeQueryDOM( $endpoint, $queryText, &$queryError )
  {
    $result = $this->executeQuery_( $endpoint, $queryText, $queryError );
    if ( $result == FALSE ) {
      return NULL;
    }
	// Given the RDF/XML results, create the new RDF document
    $rdf = new DOMDocument();
    $rdf->substituteEntities = TRUE;
    $this->curRDFDoc = NULL;
    if( !$rdf->loadXML( $result ) )
	{
	  $queryError = "<p>TW SPARQL Error: SPARQL Endpoint did not return a valid XML document</p>" ;
      return NULL;
    }
    return $rdf;
  }

  /** execute the query against the sparql endpoint, getting back an RDF
   * document
   *
   * We could already have the RDF document, if the query URI is the
   * same as the last sparql tag, or the query result RDF could be
   * cached, or we go ahead and execute the query and create the RDF
   * document.
   *
   * The resulting RDF document is stored in $this->curRDFDoc
   *
   * @param string $queryError passed by reference, store any error
   * messages in this string
   */
  public function executeQuery( &$queryError )
  {
	// if the query URI for this sparql tag is the same as the last one,
	// and we already have an RDF document, then use it. No need to run
	// the query again.
	if( ( $this->curTag["queryUri"] == $this->curQueryUri ) &&
	    ( $this->curTag["endpoint"] == $this->curEndpoint ) &&
		$this->curRDFDoc != NULL )
	{
		TWSparql::getLogger()->logStatus( "ALREADY HAVE THE QUERY RESULT" ) ;
		return ;
	}

    $this->curRDFDoc = NULL;

	// grab the endpoint
	$endpoint = $this->curTag["endpoint"] ;

    $result = $this->executeQuery_( $endpoint, $this->curQueryText, $queryError );
    if( $result == FALSE ) return;

    TWSparql::$cache->cacheQueryResult( $this->pageid,
                                        $this->curTag["queryUri"],
                                        $this->curTag["endpoint"],
                                        $result ) ;

	// Given the RDF/XML results, create the new RDF document
    $rdf = new DOMDocument();
    $rdf->substituteEntities = TRUE;
    if( !$rdf->loadXML( $result ) )
	{
	  $queryError = "<p>TW SPARQL Error: SPARQL Endpoint did not return a valid XML document</p>" ;
      return ;
    }

    $this->curRDFDoc = $rdf;
  }
  
  /** given an xslt uri, go get the xslt document
   *
   * The document either already exists, so we don't have to go load it
   * again, it is cached, or we have to get it again from the URI
   *
   * The resulting XSLT Document is stored in $this->curXslDoc
   *
   * @param string $xsltError passed by reference, store any error
   * messages in this string
   */
  public function getXsltDoc( &$xsltError )
  {
	// if the xslt is the same in this sparql tag as the last, and we
	// already have an XSLT Document, then just use it.
	if( ( $this->curTag["xsltUri"] == $this->curXsltUri ) && $this->curXsltDoc != NULL )
	{
	  return ;
	}

	$this->curXsltUri = NULL ;
	$xslt = NULL ;

    $xslt = new DOMDocument();
    $xslt->substituteEntities = TRUE;
    $xsltFullUrl = $this->curTag["xsltUri"] ;
    if( $this->curReferer )
    {
        $xsltFullUrl .= "?r=" . urlencode( $this->curReferer ) ;
    }
    //if( !$xslt->load( $this->curTag["xsltUri"] ) )
    if( !$xslt->load( $xsltFullUrl ) )
	{
      $xsltError = "<p>TW SPARQL Error: XSL Transformation failed. Check the error logs for more info.</p>" ;
      return ;
    }

    $this->curXsltUri = $this->curTag["xsltUri"] ;
	$this->curXsltDoc = $xslt ;

    return ;
  }
  
  /** given the query results and the xslt document, transform the
   * results into xhtml
   *
   * @param string $transformError passed by reference, store any error
   * messages in this string
   * @return string the resultiing xhtml
   */
  public function transform( &$transformError )
  {
    $proc = new XSLTProcessor();
    if( $this->shouldEnablePHPExtensions() )
	{
      $proc->registerPHPFunctions() ;
    }
    $proc->importStylesheet( $this->curXsltDoc ) ;
    
    $xhtml = $proc->transformToXML( $this->curRDFDoc ) ;
    
    if( !$xhtml )
	{
      $transformError = "<p>TW SPARQL Error: XSL Transformation failed. "
	                    . "Check the error logs for more info.</p>" ;
	  return "" ;
    }

    return $xhtml ;
  }
  
  // @todo do we need this function?
  public function outputResults($type, $results) {
    if(isset($_SERVER['HTTPS']) && ($_SERVER['HTTPS'] == 'on')) {
      header("Cache-Control: private");
      header("Pragma: private");
    }
    
    $type = preg_replace('/\r?\n(?!\t| )/', '', $type);
    header("Content-Type: $type");
    print $results;
    exit();
  }
  
  /** Clear all the cached information, query, rdf, xslt, and final
   * result
   */
  public function clearCache() {
    $cache = TWSparql::getCache();
    if($cache != NULL) {
      $cache->clearQueryResultCache(NULL, NULL, NULL);
      $cache->clearFinalCache(NULL, NULL, NULL, NULL);
    }
  }

  /** Simple function that simply dumps the string variables for this
   * class
   */
  private function dump()
  {
    TWSparql::getLogger()->logStatus( "TWSparql dump" ) ;
    TWSparql::getLogger()->logStatus( "&nbsp;&nbsp;&nbsp;&nbsp;curQueryUri = $this->curQueryUri" ) ;
    TWSparql::getLogger()->logStatus( "&nbsp;&nbsp;&nbsp;&nbsp;curQueryText = $this->curQueryText" ) ;
    TWSparql::getLogger()->logStatus( "&nbsp;&nbsp;&nbsp;&nbsp;curXsltUri = $this->curXsltUri" ) ;
    TWSparql::getLogger()->logStatus( "&nbsp;&nbsp;&nbsp;&nbsp;curReferer = $this->curReferer" ) ;
  }
  
  /**
   * Resolves one URI relative to another based on RFC 2396.
   * @param string $base The base URI to resolve against
   * @param string $path A path, either relative or absolute, to resolve against
   * $base.
   * @return string Absolute URI of $path
   */
  public static function rfc2396($base, $path) {
    if(0===strpos($path,"doi:")) {
      $path = "http://dx.doi.org/".substr($path, 4);
    }
    return HttpUri::parse($base)->resolve($path)->serialize();
  }

  
  // Settings
  public abstract function shouldDebug();
  public abstract function enableDebug($val);
  public abstract function shouldEnablePHPExtensions();
  public abstract function enablePHPExtensions($val);
  public abstract function getSiteBase();
  public abstract function getIbase();
  public abstract function setIbase($val);
  public abstract function getSbase();
  public abstract function setSbase($val);
  public abstract function getXsltPath();
  public abstract function setXsltPath($val);
  public abstract function getQueryPath();
  public abstract function setQueryPath($val);
  public abstract function getDefaultXsl();
  public abstract function setDefaultXsl($val);
  public abstract function getEndpoint();
  public abstract function setEndpoint($val);
  public abstract function getCacheTTL();
  public abstract function setCacheTTL($val);
  public abstract function getCacheEnabled();
  public abstract function setCacheEnabled($val);
}

