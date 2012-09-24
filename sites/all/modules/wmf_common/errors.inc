<?php

class WmfException extends Exception {
    //XXX shit we aren't using the 'rollback' attribute
    // and it's not correct in most of these cases
    static $error_types = array(
        'CIVI_CONFIG' => array(
            'fatal' => TRUE,
        ),
        'STOMP_BAD_CONNECTION' => array(
            'fatal' => TRUE,
        ),
        'INVALID_MESSAGE' => array(
            'reject' => TRUE,
        ),
        'CIVI_REQ_FIELD' => array(
            'reject' => TRUE,
        ),
        'IMPORT_CONTACT' => array(
            'fatal' => TRUE,
        ),
        'IMPORT_CONTRIB' => array(
            'fatal' => TRUE,
        ),
        'IMPORT_SUBSCRIPTION' => array(
            'fatal' => TRUE,
        ),
        'IMPORT_TAG' => array(
            'fatal' => TRUE,
        ),
        'DUPLICATE_CONTRIBUTION' => array(
            'reject' => TRUE,
            'no-email' => TRUE,
        ),
        'GET_CONTRIBUTION' => array(
            'reject' => TRUE,
        ),
        'PAYMENT_FAILED' => array(
            'no-email' => TRUE,
        ),
        'UNKNOWN' => array(
            'fatal' => TRUE,
        ),
        'UNSUBSCRIBE' => array(),
        'UNSUBSCRIBE_WARN' => array(
          'no-email' => TRUE,
        ),
    );

    var $type;

    function __construct( $type, $message, $extra = null ) {
        if ( !array_key_exists( $type, self::$error_types ) ) {
            $message .= ' -- ' . t( 'Warning, throwing a misspelled exception: "%type"', array( '%type' => $type ) );
            $type = 'UNKNOWN';
        }
        $this->type = $type;

        if ( is_array( $message ) ) {
            $message = implode( "\n", $message );
        }
        if ( $extra ) {
            $message .= "\nSource: " . var_export( $extra, true );
        }
        $this->message = htmlspecialchars(
            "{$this->type} {$message}",
            ENT_COMPAT,
            'UTF-8',
            false
        );

        if ( function_exists( 'watchdog' ) ) {
            // It seems that dblog_watchdog will pass through XSS, so
            // rely on our own escaping above, rather than pass $vars.
            watchdog( 'wmf_common', $this->message, NULL, WATCHDOG_ERROR );
        }
        if ( function_exists('drush_set_error') && $this->isFatal() ) {
            drush_set_error( $this->type, $this->message );
        }
    }

    function getErrorName()
    {
        return $this->type;
    }

    function isRollbackDb()
    {
        return $this->getErrorCharacteristic('rollback', FALSE);
    }

    function isRejectMessage()
    {
        return $this->getErrorCharacteristic('reject', FALSE);
    }

    function isFatal()
    {
        return $this->getErrorCharacteristic('fatal', FALSE);
    }

    function isNoEmail()
    {
        return $this->getErrorCharacteristic('no-email', FALSE);
    }

    protected function getErrorCharacteristic($property, $default)
    {
        $info = self::$error_types[$this->type];
        if (array_key_exists($property, $info)) {
            return $info[$property];
        }
        return $default;
    }
}