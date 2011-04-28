<?php 
/**
 * Common functions for queue2civicrm modules 
 */

// include the database switching class
require_once( drupal_get_path( 'module', 'queue2civicrm' ) . '/queue2civicrm_db_switcher.inc' );

/**
 * Connect using the Stomp library.
 */
function _queue2civicrm_stomp_connection() {
  static $con = NULL;
  
  if (!isset($con)) {
    require_once drupal_get_path('module', 'queue2civicrm') . '/Stomp.php';
    watchdog('queue2civicrm', 'Attempting connection to queue server: ' . variable_get('queue2civicrm_url', 'tcp://localhost:61613'));
    
    $con = new Stomp(variable_get('queue2civicrm_url', 'tcp://localhost:61613'));
    
    try {
      $con->connect();
      register_shutdown_function('_queue2civicrm_stomp_disconnect');
    }
    catch (Stomp_Exception $e) {
      $con = FALSE;
      watchdog('queue2civicrm', 'Queue connection failed: ' . $e->getMessage(), array(), WATCHDOG_ERROR);
    }
  }
  
  return $con;
}

/**
 * Disconnect. Only used as a shutdown function.
 */
function _queue2civicrm_stomp_disconnect() {
  $con = _queue2civicrm_stomp_connection();
  $con->disconnect();
}

/**
 * Normalize the queued message
 * 
 * Decodes the message and updates some of the message fields in ways 
 * that are consistent with how we need to insert data into Civi.  
 * This should be useful by other modules processing contribution messages 
 * out of the queue.
 * 
 * @param mixed $msg
 * @return array
 */
function _queue2civicrm_normalize_msg( $msg ) {
  // Decode the message body.
  if ( !is_array( $msg ) ) {
    $msg = json_decode($msg->body, true);
  }

  // hack to ignore msgs in the queue w/o a contribution tracking id
  if ( !isset( $msg[ 'contribution_tracking_id' ] )) {
    watchdog( 'queue2civicrm', 'Contribution missing contribution_tracking_id' );
    return FALSE;
  }

  // Convert times to Unix timestamps.
  if (!is_integer($msg['date'])) {
    $msg['date'] = strtotime($msg['date']);
  }
	
  watchdog('queue2civicrm', 'Contribution (pre-conversion):<pre>' . check_plain(print_r($msg, TRUE)) . '</pre>');
  
  $msg[ 'gateway_txn_id_orig' ] = $msg[ 'gateway_txn_id' ];
  $msg['gateway_txn_id'] .= ' ' . time();
  
  return $msg;
}

/**
 * Normalize contribution amounts
 * 
 * Do exchange rate conversions and set appropriate fields for CiviCRM
 * based on information contained in the message.
 * 
 * @param $msg
 * @return array
 */
function _queue2civicrm_normalize_contrib_amnts( $msg ) {
  // round the amounts and do exchange rate conversion
  $msg['fee'] = round( exchange_rate_convert($msg['original_currency'], $msg['fee'], $msg['date']), 2 );
  $msg['gross'] = round( exchange_rate_convert($msg['original_currency'], $msg['gross'], $msg['date']), 2 );
  $msg['net'] = round( exchange_rate_convert($msg['original_currency'], $msg['net'], $msg['date']), 2 );
  
  return $msg;
}

/**
 * Insert the contact record
 * 
 * Serves as a standard way for message porcessors to handle contact
 * insertion.
 * 
 * @param array $msg
 * @return array
 */
function _queue2civicrm_contact_insert( $msg ) {
  require_once 'api/v2/Contact.php';
  
  // Insert the contact record
  $contact = array(
	'id' => NULL,
	'contact_type' => 'Individual',
	'first_name' => $msg['first_name'],
	'middle_name' => $msg['middle_name'],
	'last_name' => $msg['last_name'],
	'do_not_trade' => ($msg['anonymous'] ? 1 : 0 ),
	'contact_source' => 'online donation'
  );
  
  // Honor the opt-out checkbox, if present
  if (array_key_exists('optout', $msg)) {
    $contact['is_opt_out'] = $msg['optout'];
  }
  $contact['sort_name'] = trim($contact['last_name'] . ', ' . $contact['first_name'], ' ,');
  $contact['display_name'] = trim($contact['first_name'] . ' ' . $contact['last_name']);
  $contact_result = &civicrm_contact_add( $contact );
  
  watchdog( 'queue2civicrm', 'Result for adding contact: %contact', array( '%contact' => print_r( $contact_result, true )), WATCHDOG_DEBUG );
  
  return $contact_result;
}

/**
 * Update the contact record
 * 
 * Serves as a standard way for message porcessors to handle contact
 * updates.
 * 
 * @param array $msg
 * @return array
 */
function _queue2civicrm_contact_update( $msg, $contact_id ) {
  require_once 'api/v2/Contact.php';
  
  // Insert the contact record
  $contact = array(
	'contact_id' => $contact_id,
    'contact_type' => 'Individual',
	'first_name' => $msg['first_name'],
	'middle_name' => $msg['middle_name'],
	'last_name' => $msg['last_name'],
  );
  
  // Honor the opt-out checkbox, if present
  if (array_key_exists('optout', $msg)) {
    $contact['is_opt_out'] = $msg['optout'];
  }
  $contact['sort_name'] = trim($contact['last_name'] . ', ' . $contact['first_name'], ' ,');
  $contact['display_name'] = trim($contact['first_name'] . ' ' . $contact['last_name']);
  $contact_result = &civicrm_contact_update( $contact );
  
  watchdog( 'queue2civicrm', 'Result for updating contact: %contact', array( '%contact' => print_r( $contact_result, true )), WATCHDOG_DEBUG );
  
  return $contact_result;
}

/**
 * Insert the location record
 * 
 * Serves as a standard way for message porcessors to handle location
 * insertion.
 * 
 * @param array $msg
 * @param array $contact
 * @return array
 */
function _queue2civicrm_location_insert( $msg, $contact ) {
  require_once 'api/v2/Location.php';
  $params = _queue2civicrm_location_prepare_params( $msg, $contact );
  $location_result = &civicrm_location_add( $params );
  watchdog( 'queue2civicrm', 'Result for adding location: %location', array( '%location' => print_r( $location_result, true )), WATCHDOG_DEBUG );
  return $location_result;  
}

/**
 * Update the location record
 * 
 * Serves as a standard way for message porcessors to handle location
 * updates.
 * 
 * @param array $msg
 * @param array $contact
 * @return array
 */
function _queue2civicrm_location_update( $msg, $contact ) {
  require_once 'api/v2/Location.php';
  $params = _queue2civicrm_location_prepare_params( $msg, $contact );
  $location_result = &civicrm_location_update( $params );
  watchdog( 'queue2civicrm', 'Result for updating location: %location', array( '%location' => print_r( $location_result, true )), WATCHDOG_DEBUG );
  return $location_result;  
}

/**
 * Prepare the parameters for location adding/updating
 * 
 * @param $msg
 * @param $contact
 * @return array
 */
function _queue2civicrm_location_prepare_params( $msg, $contact ) {
  $emails = array( 
    array(
      'email' => $msg[ 'email' ],
      'location_type_id' => 1,
    ) 
  );
  
  $address = array(
    1 => array(
	  'location_type_id' => 1, //Home
      'is_primary' => 1,
	  'street_address' => $msg['street_address'],
	  'supplemental_address_1' => $msg['supplemental_address_1'],
	  'city' => $msg['city'],
	  'state_province' => _queue2civicrm_get_state( $msg[ 'country' ], $msg['state_province'] ),
	  'postal_code' => $msg['postal_code'],
	  'country' => $msg['country'],
	  'is_billing' => 1,
    )
  );
  
  $params = array(
    'version' => '3.0',
    'contact_id' => $contact[ 'contact_id' ],
    'address' => $address,
    'email' => $emails
  );
  
  return $params;
}

/**
 * Insert the contribution record
 * 
 * Serves as a standard way for message porcessors to handle contribution
 * insertion.
 * 
 * @param array $msg
 * @param array $contact
 * @return array
 */
function _queue2civicrm_contribution_insert( $msg, $contact, $recur_id=NULL ) {
  require_once 'api/v2/Contribute.php';
  
  $trxn_id = strtoupper( $msg[ 'gateway' ] . ' ' . $msg[ 'gateway_txn_id' ] );
  if ( $recur_id ) {
    $trxn_id = 'RECURRING ' . $trxn_id; 
  }
  
  // Insert the contribution record
  $contribution = array(
	'contact_id' => $contact[ 'contact_id' ],
	'total_amount' => $msg['gross'],
	'contribution_type_id' => 9, // cash donation @fixme this needs to be pulled from a variable that was available with fundcore, but no longer exists
	'payment_instrument_id' => 6, // contribution @fixme see above
	'fee_amount' => $msg['fee'],
	'net_amount' => $msg['net'],
	'trxn_id' => $trxn_id,
	'receive_date' => date('Y-m-d H:i:s', $msg['date']),
	'currency' => 'USD',
	'source' => $msg['original_currency'] . ' ' . $msg['original_gross'],
    'contribution_recur_id' => $recur_id
  );

  watchdog( 'queue2civicrm', 'Contribution array for civicrm_contribution_add(): ' . print_r($contribution, TRUE));
  
  $contribution_result = civicrm_contribution_add( $contribution );
  
  watchdog( 'queue2civicrm', 'Contribution result from civicrm_contribution_add(): ' . print_r($contribution_result, TRUE));
  
  /**
   *  Apply custom field defaults.
   *
   * Civicrm API v2 does NOT provide methods for custom groups/fields beyond creation and deleition,
   * so we hack this cusotm.  Hopefully this won't be forever...
   *
   * At least we can use the CiviCRM DAO stuff to stuff necessary custom data in to the contribution object.
   * 
   * At the moment, all custom fields get default values for online contribution EXCEPT for 'Donor Comment'.
   */
  $dbs = _queue2civicrm_get_dbs();
  $dbs->use_civicrm();
  $query = "SELECT id, table_name FROM civicrm_custom_group WHERE extends='Contribution' && is_active='1';";
  $dao = CRM_Core_DAO::executeQuery( $query ); // Execute's query using CiviCRM data object stuff
  while ( $dao->fetch() ) {
    if ( $dao->name == 'Contribution_Comments' ) {
      $comment = ( $msg[ 'comment' ] ) ? $msg[ 'comment'] : '';
      $query = "INSERT INTO {" . $dao->table_name . "} (entity_id, donor_comment) VALUES ( %d, '%s' )";
      db_query( $query, $contribution_result[ 'id' ], $comment );
    } else { // if we dont make sure $dao->default_value has some value, Civi breaks when we try to insert
      $query = "INSERT INTO {" . $dao->table_name . "} (entity_id) VALUES (%d)";
      db_query( $query, $contribution_result[ 'id' ] );
    }
  }
  $dbs->use_default();
  
  return $contribution_result;
}

/**
 * Map contribution to tracking record in contribution_tracking table
 * 
 * @param array $msg
 * @param array $contribution
 */
function _queue2civicrm_update_contribution_tracking( $msg, $contribution ) {
  if (array_key_exists( 'contribution_tracking_id', $msg )) {
    $result = db_query('UPDATE {contribution_tracking} SET contribution_id=%d WHERE id=%d', $contribution['id'], $msg['contribution_tracking_id']);
    if ( !$result ) {
      watchdog( 'queue2civicrm', "There was a probme updating contribution_tracking for message: %msg", array( '%msg' => print_r( $msg, true )), WATCHDOG_ERROR );
      return FALSE;
    } else {
      watchdog( 'queue2civicrm', 'Successfuly updated contribution_tracking for message: %msg', array( '%msg' => print_r( $msg, true)));
      return TRUE;
    }
  }
}

/**
 * Insert the tag record
 * 
 * Serves as a standard way for message porcessors to handle tag
 * insertion.
 * 
 * @param array $contact
 * @return array
 */
function _queue2civicrm_tag_insert( $contact ) {
  require_once 'api/v2/EntityTag.php';
  
  $tag = array(
	'tag_id' => 7, // review tag @fixme should this also be variable?
	'contact_id' => $contact[ 'contact_id' ]
  );
  $tag_result = &civicrm_entity_tag_add( $tag );
  return $tag_result;
}

/**
 * Find correct state for insertion
 * 
 * When passing CiviCRM a state abbreviation, odd things can happen - like getting the right abbreviation, but the wrong state
 * So we'll pull back the correct state/province name based off of a user's country/state abbreviation
 */
function _queue2civicrm_get_state( $country, $state ) {
  
  if ( strlen( $country ) == 2 ) {
    $query = "SELECT s.name AS state_name FROM civicrm_country c, civicrm_state_province s WHERE s.country_id=c.id AND c.iso_code='" . addslashes( $country ) . "' AND s.abbreviation='" . addslashes( $state ) . "'";
  } else {
    $query = "SELECT s.name AS state_name FROM civicrm_country c, civicrm_state_province s WHERE s.country_id=c.id AND c.name='" . addslashes( $country ) . "' AND s.abbreviation='" . addslashes( $state ) . "'";
  }
  $dao = CRM_Core_DAO::executeQuery( $query );
  while ( $dao->fetch() ) {
    $state = ( !is_null( $dao->state_name ) ) ? $dao->state_name : $state;
  }
  
  return $state;
}

/**
 * Manage the database switching object
 * @return object
 */
function _queue2civicrm_get_dbs() {
  static $dbs = NULL;
  
  if ( !$dbs ) {
    $dbs = new queue2civicrm_db_switcher();
  }
  
  return $dbs;
}

/**
 * Log a transaction to queue2civicrm_log in the database
 * 
 * The array needs to contain the following keys:
 *  gateway, gateway_txn_id, data, timestamp, verified
 * And optionally:
 *  cid (which is the ID of the log record to be updated)
 * 	
 * If 'cid' is present, this will update the log record, otherwise
 * this will insert a new record.
 * 
 * @param $log
 * @return mixed false on fail, cid on insert, otherwise true
 */
function _queue2civicrm_log( $log=array() ) {
  if ( empty( $log ) ) {
    return false;  
  }
  
  // make sure we're using the default db
  $dbs = _queue2civicrm_get_dbs();
  $dbs->use_default();
  
  // if cid is set in the log array, we need to update
  if ( $log[ 'cid' ] ) {
    $query = "UPDATE {queue2civicrm_log} SET gateway='%s', gateway_txn_id='%s', data='%s', timestamp=%d, verified=%d WHERE cid=%d";
    $result = db_query( $query, $log[ 'gateway' ], $log[ 'gateway_txn_id' ], $log[ 'data' ], $log[ 'timestamp' ], $log[ 'verified' ], $log[ 'cid' ] );
    if ( $result ) $result = true;
  } else {  
    $query = "INSERT INTO {queue2civicrm_log} ( gateway, gateway_txn_id, data, timestamp, verified ) VALUES ( '%s', '%s', '%s', %d, %d )";
    $result = db_query( $query, $log[ 'gateway' ], $log[ 'gateway_txn_id' ], $log[ 'data' ], $log[ 'timestamp' ], $log[ 'verified' ] );
    if ( $result ) {
      $result = db_last_insert_id( 'queue2civicrm_log', 'cid' );
    }
  }
  
  if ( !$result ) {
    watchdog( 'queue2civicrm', 'Failed logging the transaction: %log', array( "%log" => print_r( $log, true )), WATCHDOG_ERROR );
  }
  return $result;
}