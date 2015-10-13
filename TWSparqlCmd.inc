<?php

/*
 * To change this template, choose Tools | Templates
 * and open the template in the editor.
 */

include_once('TWSparql.inc');

/**
 * Defines a logger object for TW Sparql module to provide feedback to
 * users specific to Drupal.
 * @author pattoe 
 * @author pwest
 */
class TWSparqlCmdLogger implements TWSparqlLogger {
  public $debugEnabled = FALSE ;
  private $logHandle = NULL ;
  private $errorHandle = NULL ;

  public static function init() {
    TWSparql::$logger = new TWSparqlCmdLogger();
  }
  
  /**
   * Return whether debugging is turned on or not
   */
  public function shouldDebug() {
    return $this->debugEnabled ;
  }

  /**
   * Set whether or not we should debug
   */
  public function enableDebug($val) {
	// if already turned on and turning on, nothing to do
	if( $this->debugEnabled && $val )
	{
		return ;
	}
	// if already turned off and turning off, nothing to do
	if( !$this->debugEnabled && !$val )
	{
		return ;
	}
	// if already turned on and turning off, then close the log file
	if( $this->debugEnabled && !$val )
	{
		if( $this->logHandle )
		{
			fclose( $this->logHandle ) ;
			$this->logHandle = NULL ;
		}
		if( $this->errorHandle )
		{
			fclose( $this->errorHandle ) ;
			$this->errorHandle = NULL ;
		}
	}
	else
	{
		// if already turned off and turning on, then open the log file
		if( $this->logHandle )
		{
			fclose( $this->logHandle ) ;
		}
		$this->logHandle = fopen( "./twsparql.log", "a+" ) ;
		if( $this->errorHandle )
		{
			fclose( $this->errorHandle ) ;
		}
		$this->errorHandle = fopen( "./twsparql_error.log", "a+" ) ;
	}

	$this->debugEnabled = $val ;
  }

  /**
   * Logs a string as an error.
   * @param string $str String to log to the error console. 
   */
  public function logError($str) {
	if( $this->shouldDebug() ) {
	  fwrite( $this->errorHandle, $str . "\n" ) ;
	}
  }
  
  /**
   * Logs a string as a warning
   * @param string $str String to log to the warning console. 
   */
  public function logWarning($str) {
	if( $this->shouldDebug() ) {
	  fwrite( $this->logHandle, $str . "\n" ) ;
	}
  }
  
  /**
   * Logs a string as a status
   * @param string $str String to log to the status console. 
   */
  public function logStatus($str) {
	if( $this->shouldDebug() ) {
	  fwrite( $this->logHandle, $str . "\n" ) ;
	}
  }
}

/**
 * Defines Drupal specific cache object for the TWSparql module to cache intermediate and final results of a query.
 * @author pattoe
 * @author pwest
 */
class TWSparqlCmdCache implements TWSparqlCache {
  public static function init() {
    TWSparql::$cache = new TWSparqlCmdCache();
  }
  
  /**
   * Caches a query document for a particular URI
   * @param string $queryUri A URI that 'executes' the query file to get
   * the query text
   * @param string $results The query text returned by dereferencing $queryUri 
   */
  public function cacheQueryResult($pageid, $queryUri, $endpoint,
								   $results, $expires) {
  }
  
  /**
   * Caches the final transformation of a query using an XSL template.
   * @param string $queryUri The URI used to generate the SPARQL results
   * @param string $xsltUri The URI used to dereference the XSL stylesheet
   * @param string $result The final result applying the transformation named by
   * $xsltUri to the document returned by deferencing $queryUri.
   * @param string $expires An optional duration to wait before expiring the
   * result; defaults to TWSparql::getEngine()->getCacheTTL()
   */
  public function cacheFinal($pageid, $queryUri, $endpoint, $xsltUri,
							 $result, $expires) {
  }
  
  /**
   * Clears the cached copy of $queryUri, if any.
   * @param string $queryUri 
   */
  public function clearQueryResultCache($queryUri, $endpoint) {
  }

  public function clearFinalCache($queryUri, $endpoint, $xsltUri) {
  }
  
  /**
   * Gets the cached content for $queryUri, or NULL if no content is available.
   * @param string $queryUri 
   */
  public function getCachedQueryResult($pageid, $queryUri, $endpoint) {
    return NULL ;
  }

  public function getCachedFinal($pageid, $queryUri, $endpoint, $xsltUri) {
	return NULL ;
  }
}

class TWSparqlCmd extends TWSparql {
  private $phpExtensions ;
  private $iBase ;
  private $sBase ;
  private $queryPath ;
  private $xsltPath ;
  private $endpoint ;

  public static function init() {
    TWSparql::$engine = new TWSparqlCmd();
  }
  
  public function shouldDebug() {
    return TWSparql::getLogger()->shouldDebug() ;
  }
  
  public function enableDebug($val) {
    TWSparql::getLogger()->enableDebug($val) ;
  }
  
  public function shouldEnablePHPExtensions() {
    return $this->phpExtensions ;
  }
  
  public function enablePHPExtensions($val) {
    $this->phpExtensions = $val ;
  }
  
  public function getSiteBase() {
	return "" ;
  }

  public function getIbase() {
    return $this->iBase ;
  }
  
  public function setIbase($val) {
    $this->iBase = $val ;
  }
  
  public function getSbase() {
    return $this->sBase ;
  }
  
  public function setSbase($val) {
    $this->sBase = $val ;
  }
  
  public function getXsltPath() {
    return $this->xsltPath ;
  }
  
  public function setXsltPath($val) {
    $this->xsltPath = $val ;
  }
  
  public function getQueryPath() {
    return $this->queryPath ;
  }
  
  public function setQueryPath($val) {
    $this->queryPath = $val ;
  }
  
  public function getDefaultXsl() {
    return "" ;
  }
  
  public function setDefaultXsl($val) {
  }
  
  public function getEndpoint() {
    return $this->endpoint ;
  }
  
  public function setEndpoint($val) {
	$this->endpoint = $val ;
  }
  
  public function getCacheTTL() {
    return "" ;
  }
  
  public function setCacheTTL($val) {
  }
  
  public function getCacheEnabled() {
    return FALSE ;
  }
  
  public function setCacheEnabled($val) {
  }
}

// Could have just used the TWSparqlCmd static function init to
// initialize all three of these, but could be the case where someone
// wants to use different loggers and caching mechanisms instead of the
// Drupal one here.
TWSparqlCmd::init();
TWSparqlCmdLogger::init();
TWSparqlCmdCache::init();

