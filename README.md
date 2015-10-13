# twsparql
PHP code to query sparql endpoint and translate results to html

This is the common php code that can be used by more specific clients, such as PHP HTML, Drupal and Mediawiki to query sparql endpoints and translate the results of the query to html.

The query file is treated like a php script so you can pass in variables to the query, such as a specific URI.

Currently only supports the return of RDF/XML and uses XSL to translate the results to HTML.
