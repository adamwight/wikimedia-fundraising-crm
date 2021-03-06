<?php

/**
 * Loads the column headers into an array so that they can be used independent
 * of the column order when generating messages
 *
 * @param $row Array containing the column headers for the csv
 * @return Array mapping the header keys to the column index
 */
function _load_headers( $row ){
  $header_keys = array();

  # trimming the " from each side
  foreach( $row as $i => $k ) {
    $header_keys[trim( $k, '"' )] = $i;
  }
  watchdog('offline2civicrm', 'Import checks: Column headers loaded from file', array(), WATCHDOG_INFO);

  return $header_keys;
}

function _get_value( $column, $row, $headers, $default='' ){

  if( !array_key_exists( $column, $headers ) ){
    return $default;
  } else {
    $value = trim( $row[ $headers[ $column ] ] );
    if ( empty( $value ) ) {
      return $default;
    }
    return $value;
  }
}
