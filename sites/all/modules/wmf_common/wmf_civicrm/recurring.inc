<?php

/**
 * Insert the recurring contribution record
 * 
 * @todo Determine the watchdog level for errors.
 * @todo Are the dates being set with the correct value?
 * 
 * @param array $msg
 * @param integer $contact_id
 * @param array $contribution   The contribution record
 * @return boolean	Returns true on success
 */
function wmf_civicrm_message_contribution_recur_insert( $msg, $contact_id, $contribution_record )
{
    $msg[ 'frequency_unit' ] = isset( $msg[ 'frequency_unit' ] ) ? $msg[ 'frequency_unit' ] : 'month';
    $msg[ 'frequency_interval' ] = isset( $msg[ 'frequency_interval' ] ) ? (integer) $msg[ 'frequency_interval' ] : 1;
    $msg[ 'installments' ] = isset( $msg[ 'installments' ] ) ? (integer) $msg[ 'installments' ] : 0;

    if ( !isset( $msg[ 'recurring' ] ) && $msg[ 'recurring' ] != 1 ) {
        $error_message = t(
            'Payment is not marked as recurring, with the contact_id [!contact_id]',
            array( "!contact_id" => $contact_id )
        );
        throw new WmfException( 'IMPORT_SUBSCRIPTION', $error_message );
      }
    
    // Allowed frequency_units
    $frequency_units = array( 'month' );
    if ( !in_array( $msg[ 'frequency_unit' ], $frequency_units ) ) {
        $error_message = t(
            'Invalid `frequency_unit` specified [!frequency_unit]. Supported frequency_units: !frequency_units, with the contact_id [!contact_id]',
            array(
                "!frequency_unit" => $msg[ 'frequency_unit' ],
                "!frequency_units" => implode( ', ', $frequency_units ),
                "!contact_id" => $contact_id,
            )
        );
        throw new WmfException( 'IMPORT_SUBSCRIPTION', $error_message );
    }

    // Frequency interval is only allowed to be 1. FIXME
    if ( $msg[ 'frequency_interval' ] !== 1 ) {
        $error_message = t(
            '`frequency_interval` is only allowed to be set to 1, with the contact_id [!contact_id]',
            array("!contact_id" => $contact_id)
        );
        throw new WmfException('IMPORT_SUBSCRIPTION', $watchdog_message );
    }

    // installments is only allowed to be 0.
    if ( $msg[ 'installments' ] !== 0 ) {
        $error_message = t(
            '`installments` must be set to 0, with the contact_id [!contact_id]',
            array("!contact_id" => $contact_id)
        );
        throw new WmfException('IMPORT_SUBSCRIPTION', $error_message );
    }
    
    if ( empty( $contribution_record ) ) {
        $error_message = t(
            'The contribution_record is empty, with the contact_id [!contact_id]',
            array("!contact_id" => $contact_id)
        );
        throw new WmfException('IMPORT_SUBSCRIPTION', $error_message );
    }

    // trxn_id must be set.
    if ( empty( $contribution_record[ 'trxn_id' ] ) ) {
        $error_message = t(
            '`trxn_id` must be set and not empty, with the contact_id [!contact_id]',
            array("!contact_id" => $contact_id)
        );
        throw new WmfException('IMPORT_SUBSCRIPTION', $error_message );
    }
    
    $msg['cycle_day'] = wmf_civicrm_get_cycle_day( $msg['date'] );
    
    $next_sched_contribution = wmf_civicrm_get_next_sched_contribution_date_for_month( $msg );
    
    $dbs = wmf_civicrm_get_dbs();
    $dbs->push( 'civicrm' );

    $contribution_recur_id = db_insert( 'civicrm_contribution_recur' )->fields( array(
        'contact_id' => $contact_id,
        'amount' => $msg[ 'original_gross' ],
        'currency' => $msg[ 'original_currency' ],
        'frequency_unit' => $msg[ 'frequency_unit' ],
        'frequency_interval' => $msg[ 'frequency_interval' ],
        'installments' => $msg[ 'installments' ],
        'start_date' => wmf_common_date_unix_to_civicrm( $msg[ 'date' ] ),
        'create_date' => wmf_common_date_unix_to_civicrm( $msg[ 'date' ] ),
        'cancel_date' => ($msg['cancel'] ? wmf_common_date_unix_to_civicrm( time() ) : null ),
        #FIXME:
        'processor_id' => 1,
        'cycle_day' => $msg[ 'cycle_day' ],
        'next_sched_contribution' => $next_sched_contribution,
        'trxn_id' => $contribution_record['trxn_id'],
    ) )->execute();

    $dbs->pop();
    
    // Link the contribution to the recurring payment
    //TODO catch error and print message
    $link_contribution_status = wmf_civicrm_update_contribution_for_recurring( $contribution_record['id'], $contribution_recur_id );
    
    return $link_contribution_status;
}

/**
 * Link a recurring payment to the contribution record
 * 
 * @param integer $contribution_id
 * @param integer $contribution_recur_id
 */
function wmf_civicrm_update_contribution_for_recurring( $contribution_id, $contribution_recur_id )
{
    // contribution_id must be set and not null or zero
    if ( empty( $contribution_id ) ) {
        $error_message = t('`contribution_id` must be set and not empty.');
        throw new WmfException('IMPORT_SUBSCRIPTION', $error_message);
    }
    
    // contribution_recur_id must be set and not null or zero
    if ( empty( $contribution_recur_id ) ) {
        $error_message = t('`contribution_recur_id` must be set and not empty.');
        throw new WmfException('IMPORT_SUBSCRIPTION', $error_message);
    }

    $dbs = wmf_civicrm_get_dbs();
    $dbs->push( 'civicrm' );
    $result = db_update( 'civicrm_contribution' )->fields( array(
        'contribution_recur_id' => $contribution_recur_id,
    ) )->condition( 'id', $contribution_id )->execute();
    $dbs->pop();

    if ( !$result ) {
        $error_message = t(
            'There was a problem linking the contribution [!contribution_id] to the recurring contribution [!contribution_recur_id]',
            array(
                '!contribution_id' => $contribution_id,
                '!contribution_recur_id' => $contribution_recur_id
            )
        );
        throw new WmfException('IMPORT_SUBSCRIPTION', $error_message);
    } else {
        watchdog( 'wmf_civicrm', 'Successfully linked the contribution  [!contribution_id] to the recurring contribution [!contribution_recur_id]', array( '!contribution_id' => $contribution_id, '!contribution_recur_id' => $contribution_recur_id ) );
        return true;
    }
}

/**
 * Select a set of recurring payments by `next_sched_contribution`, that need to be processed for the current day.
 *
 * Example query called with standard options and the date set to: 2012-01-01
 *
 * SELECT `civicrm_contribution_recur`.* FROM `civicrm_contribution_recur`
 *  WHERE `civicrm_contribution_recur`.`next_sched_contribution`
 *   BETWEEN '2012-01-01 00:00:00' AND '2012-01-01 23:59:59'
 *  AND `civicrm_contribution_recur`.`trxn_id` LIKE 'RECURRING GLOBALCOLLECT%'
 *  AND ( `civicrm_contribution_recur`.`end_date` IS NULL )
 *  AND ( `civicrm_contribution_recur`.`failure_retry_date` IS NULL )
 *  AND `civicrm_contribution_recur`.`failure_count` = 0
 *  AND `civicrm_contribution_recur`.`contribution_status_id` = 1
 * LIMIT 1
 * 
 * options:
 * - date           string    Uses strtotime() to parse the date
 * - $limit         integer   Number of records to pull. Default is 1.
 * 
 * @todo The field `civicrm_payment_processor`.`payment_processor_type` should be set.
 * @todo Implement $contributionStatus = CRM_Contribute_PseudoConstant::contributionStatus( null, 'name' );
 * 
 * @return array of contribution_recur objects
 */
function wmf_civicrm_get_next_sched_contribution($limit = 1, $date = 'now')
{
    $date = date('Y-m-d', strtotime($date));
    
    $start_day = $date . ' 00:00:00';
    $end_day = $date . ' 23:59:59';
    
    $start_day_stamp = strtotime($start_day);
    $end_day_stamp = strtotime($end_day);

    $recurring_globalcollect_run_missed_days = (integer) variable_get('recurring_globalcollect_run_missed_days', 0);
    
    if ($recurring_globalcollect_run_missed_days > 0) {

        $decrement_date = '-' . $recurring_globalcollect_run_missed_days . ' day';
        
        $start_day = date('Y-m-d 00:00:00', strtotime($decrement_date, $start_day_stamp));
    }
    
    $contribution_status_id = civicrm_api_contribution_status('Completed');
    
    $query = <<<EOS
SELECT * FROM civicrm_contribution_recur
WHERE
    civicrm_contribution_recur.next_sched_contribution BETWEEN :start AND :end
    AND civicrm_contribution_recur.trxn_id LIKE 'RECURRING GLOBALCOLLECT%'
    AND ( civicrm_contribution_recur.end_date IS NULL )
    AND civicrm_contribution_recur.contribution_status_id = :status_id
LIMIT {$limit}
EOS;

    $dbs = wmf_civicrm_get_dbs();
    $dbs->push( 'civicrm' );

    $res = db_query( $query, array(
        ':start' => $start_day,
        ':end' => $end_day,
        ':status_id' => $contribution_status_id,
    ) );
    
    return $res->fetchAll();
}

/**
 * wmf_civicrm_get_next_sched_contribution_date_for_month
 * 
 * @return  string  Returns a date stamp in the format 'Y-m-d H:i:s' => 2011-12-31 00:00:00
 */
function wmf_civicrm_get_next_sched_contribution_date_for_month($record)
{
    $triggered_for_date = wmf_civicrm_get_last_trigger_date($record);
    //$frequency_interval and $cycle_day will, at this point, have been found in $record. 
    $frequency_interval = (integer) $record['frequency_interval'];
    $cycle_day = $record['cycle_day'];

    $scheduled_date_stamp = $triggered_for_date;
    $added = 0;
    while ( date('Y-m-d', $triggered_for_date) >= date('Y-m-d', $scheduled_date_stamp) && ( $added < $frequency_interval ) ){
        //this will happen at least once.
        $scheduled_date_stamp = wmf_civicrm_increment_date_to_target_day( $scheduled_date_stamp, 'month', $cycle_day );
        $added += 1;
    }
    
    return date('Y-m-d H:i:s', $scheduled_date_stamp);
}

/**
 * Calculates the last date this payment should have been triggered for, 
 * regardless of the actual date, or the last recorded date in the schedule. 
 * @param array $record An array that contains, at least, the cycle day. Passing 
 * this around in record format because that's what the rest of the module does. 
 * @TODO: Stop passing around the whole record. 
 * @return timestamp A midnight timestamp for the day that should have triggered 
 * this recurring transaction. 
 */
function wmf_civicrm_get_last_trigger_date($record)
{
    //Instead of adding to now, we have to look for the last time the cycle date 
    //should have been triggered, regardless of when the transaction actually went through.
    
    //TODO: This needs to implement more frequency intervals. For now, though, we only use monthly, so...
    if ( !array_key_exists( 'cycle_day', $record ) || !is_numeric( $record['cycle_day'] ) ){
        return time();
    } else {
        $cycle_day = (integer) $record['cycle_day'];
    }
    
    $nowstamp = time();
    
    $month = (int) date('n', $nowstamp);
    $year = (int) date('Y', $nowstamp);
    
    // Build a timestamp for the cycle day in this month
    // If we are still in the same month, this will be the correct value. If we're in the next month,
    // it'll be the wrong value and it'll be in the future; we fix that up below.
    $last_trigger = mktime(0, 0, 0, $month, wmf_civicrm_get_cycle_day_for_month( $cycle_day, $month, $year ), $year);
    
    // This loop fixes up $last_trigger in the case where we're running in a later month than when
    // the last trigger date actually was, which means $last_trigger will be in the future and will
    // therefore be wrong (instead, it'll be the *next* trigger date). In this case, we move
    // $last_trigger back a month. The 60*60*24*7 represents a week in seconds: if the next payment
    // is due in less than a week, do NOT execute it but skip to the next month instead.
    // TODO this is incredibly confusing, mostly because this logic is in the wrong place. It
    // should be checked when computing the next payment date, not here where we're computing the
    // previous (scheduled) payment date.
    while ( $last_trigger > $nowstamp && ( $last_trigger - $nowstamp ) > 60*60*24*7 ){
        //decrement the month until it was in the past. 
        --$month;
        if ( $month < 1 ){
            $month = 12;
            --$year;
        }
        $last_trigger = mktime(0, 0, 0, $month, wmf_civicrm_get_cycle_day_for_month( $cycle_day, $month, $year ), $year);
    }
    return $last_trigger;
}

/**
 * @param timestamp $date as unix seconds
 * @return day of the month for this date
 */
function wmf_civicrm_get_cycle_day( $date ) {
    return intval( gmdate( 'j', $date ) );
}

/**
 * Increment the $date by one $interval, landing as close as possible to 
 * $cycle_day. Have only implemented the $interval of 'month' at this point. 
 * Might wire up more later as-needed. 
 * @param timestamp $date Timestamp to increment by the interval
 * @param string $interval A name for the interval that we're incrementing. 
 * @param int $cycle_day The target day of the month for this payment
 * @return timestamp The $date parameter incremented by one calendar interval. 
 */
function wmf_civicrm_increment_date_to_target_day( $date, $interval = 'month', $cycle_day = null )
{
    switch ($interval){ //just making it slightly nicer in here for the next guy
    case 'month':
    default:
        if ( is_null( $cycle_day ) ){
          $cycle_day = wmf_civicrm_get_cycle_day( $date );
        }
        $month = (int) date('n', $date);
        $year = (int) date('Y', $date);

        $month += 1;
        //if we wanted to edit this to handle adding more than one month at 
        //a time, we could do some fun stuff with modulo here. 
        if ( $month > 12 ){
          $month = 1;
          $year += 1;
        }

        $target_day = wmf_civicrm_get_cycle_day_for_month($cycle_day, $month, $year);

        $next_date = mktime(0, 0, 0, $month, $target_day, $year);
        return $next_date;
    }
}

/**
 * @param int $cycle_day - target day of the month for this subscription
 * @param int $month - target month
 * @param int $year - target year. 
 * @return int The day of the specified month and year most appropriate for the 
 * target cycle day. This will only change if the target day doesn't exist in certain months. 
 */
function wmf_civicrm_get_cycle_day_for_month($cycle_day, $month, $year){
    $last_day = cal_days_in_month( CAL_GREGORIAN, $month, $year );
    if ( $cycle_day > $last_day ){
        return $last_day;
    }
    return $cycle_day;
}
