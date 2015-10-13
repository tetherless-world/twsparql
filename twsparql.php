<?php

include_once( "TWSparqlCmd.inc" ) ;

global $outHandle ;
$outHandle = STDOUT ;

global $usingStdout ;
$usingStdout = true ;

$shortopts = "" ;
$longopts = array( "config:",
                   "input::",
                   "enable-debug",
				   "query::",
				   "uri::",
				   "xslt::",
				   "endpoint::",
				   "output::" ) ;
$options = getopt( $shortopts, $longopts ) ;

if( !is_array( $options ) )
{
	usage( $argv, "No configuration file specified" ) ;
}

if( !isset( $options["input"] ) )
{
	if( !isset( $options["query"] ) || !isset( $options["xslt"] ) )
	{
		usage( $argv, "Must specify query and xslt if no input file specified");
	}
}

if( isset( $options["enable-debug"] ) )
{
	TWSparql::getEngine()->enableDebug( TRUE ) ;
}

if( !isset( $options["config"] ) )
{
	usage( $argv, "Must specify a configuration file" ) ;
}

if( !file_exists( $options["config"] ) )
{
	usage( $argv, "configuration file " . $options["config"]
	       . " does not exist\n" ) ;
}

$ini_array = parse_ini_file( $options["config"], true ) ;
$missing = verify_config( $ini_array ) ;
if( $missing != "" )
{
	usage( $argv, "configuration file " . $options["config"]
	       . " is missing options or options invalid: $missing\n" ) ;
}

TWSparql::getEngine()->setIbase( $ini_array["iBase"] ) ;
TWSparql::getEngine()->setSbase( $ini_array["sBase"] ) ;
TWSparql::getEngine()->setXsltPath( $ini_array["xsltPath"] ) ;
TWSparql::getEngine()->setQueryPath( $ini_array["queryPath"] ) ;

if( isset( $options["endpoint"] ) )
{
	TWSparql::getEngine()->setEndpoint( $options["endpoint"] ) ;
}
else
{
	TWSparql::getEngine()->setEndpoint( $ini_array["endpoint"] ) ;
}

if( isset( $options["output"] ) )
{
	$outHandle = fopen( $options["output"], "w" ) ;
	if( !$outHandle )
	{
		usage( $argv, "could not create output file " .  $options["output"] ) ;
	}
}

// if the input file is specified, then we just read it and use the
// files content
if( isset( $options["input"] ) )
{
	if( !file_exists( $options["input"] ) )
	{
		usage( $argv, "input file " . $options["input"]
			   . " does not exist\n" ) ;
	}

	runWithInput( $options["input"] ) ;
}
else
{
	// if no input file is specified then we need to create contents, sparql
	// tag
	runWithOptions( $options["query"], $options["uri"], $options["xslt"] ) ;
}

function usage( $argv, $err )
{
    $prog = $argv[0] ;
    print( "$prog\n" ) ;
    print( "  $err\n\n" ) ;
	print( "OPTIONS:\n" ) ;
	print( "  --config=<config_file> - contains variable definitions\n" ) ;
	print( "  --input=<input_file> - input file containing sparql tags\n" ) ;
	print( "  --output=<output_file> - file to write xhtml output to\n" ) ;
	print( "  --query=<query_uri> - URI of query file\n" ) ;
	print( "  --xslt=<xslt_uri> - URI of xslt file\n" ) ;
	print( "  --uri=<instance_uri> - instance or schema URI\n" ) ;
	print( "  --enable-debug - status and warning written to ./twsparql.log\n" ) ;
	print( "\n" ) ;
	print( "NOTES:\n" ) ;
	print( "  can specify more then one query, uri, and xslt "
	       . "- first query goes with first uri goes with first xslt\n" ) ;
	print( "  specify an input file instead of query, xslt, and uri\n" ) ;
	print( "  if no output file is specified, output written to STDOUT\n" ) ;

	cleanUp() ;

    exit( 1 ) ;
}

function cleanUp()
{
	global $outHandle, $usingStdout ;

	if( !$usingStdout )
	{
		if( $outHandle )
		{
			fclose( $outHandle ) ;
			$outHandle = NULL ;
		}
	}

}

function verify_config( $ini_array )
{
	$expected = array( "iBase",
						"sBase",
						"xsltPath",
						"queryPath",
						"endpoint",
	) ;

	$bad = false ;
	$missing = "" ;

	foreach( $expected as $var )
	{
		if( !isset( $ini_array[$var] ) )
		{
			if( $bad ) $missing .= ", " ;
			$missing .= $var ;
			$bad = true ;
		}
	}

	return $missing ;
}

function runWithInput( $input_file )
{
	global $outHandle ;

    $contents = file_get_contents( $input_file ) ;

	fwrite( $outHandle, TWSparql::getEngine()->render( null, $contents ) ) ;
}

function runWithOptions( $query, $uri, $xslt )
{
	global $outHandle ;

	$contents = "<sparql query=\"$query\" xslt=\"$xslt\" uri=\"$uri\"/>" ;

	fwrite( $outHandle, TWSparql::getEngine()->render( null, $contents ) ) ;
}

?>

