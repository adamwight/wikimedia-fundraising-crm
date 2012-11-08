<?php

namespace wmf_eoy_receipt;

require_once 'Mailer.php';
require_once 'Templating.php';
require_once 'Translation.php';

class EoySummary
{
    static protected $templates_dir;
    static protected $template_name;
    static protected $option_keys = array(
        'year',
        'test',
        'batch',
        'job_id',
    );

    protected $batch_max = 100;

    function __construct( $options = array() )
    {
        $this->year = variable_get( 'wmf_eoy_target_year', null );
        $this->batch_max = variable_get( 'wmf_eoy_batch_max', 100 );
        $this->test = variable_get( 'wmf_eoy_test_mode', TRUE );

        foreach ( self::$option_keys as $key ) {
            if ( array_key_exists( $key, $options ) ) {
                $this->$key = $options[ $key ];
            }
        }

        $this->from_address = variable_get( 'thank_you_from_address', null );
        $this->from_name = variable_get( 'thank_you_from_name', null );
        if ( !$this->from_address || !$this->from_name ) {
            throw new Exception( "Must configure a valid return address in the Thank-you module" );
        }

        // FIXME: this is not required on the production configuration.
        // However, it will require code changes if the databases are
        // actually hosted on separate servers.  You will need to specify
        // the database name: 'wmf_civi.' if you are isolating for dev.
        $this->civi_prefix = '';

        self::$templates_dir = __DIR__ . '/templates';
        self::$template_name = 'eoy_thank_you';
    }

    //FIXME rename
    function calculate_year_totals()
    {
        $job_timestamp = date( "YmdHis" );
        $year_start = strtotime( "{$this->year}-01-01 00:00:01" );
        $year_end = strtotime( "{$this->year}-12-31 23:59:59" );

        $select_query = <<<EOS
SELECT
    %d AS job_id,
    COALESCE( billing_email.email, primary_email.email ) AS email,
    contact.first_name,
    contact.preferred_language,
    'queued',
    GROUP_CONCAT( CONCAT(
        DATE_FORMAT( contribution.receive_date, '%%Y-%%m-%%d' ),
        ' ',
        contribution.total_amount,
        ' ',
        contribution.currency
    ) )
FROM {$this->civi_prefix}civicrm_contribution contribution
LEFT JOIN {$this->civi_prefix}civicrm_email billing_email
    ON billing_email.contact_id = contribution.contact_id AND billing_email.is_billing
LEFT JOIN {$this->civi_prefix}civicrm_email primary_email
    ON primary_email.contact_id = contribution.contact_id AND primary_email.is_primary
JOIN {$this->civi_prefix}civicrm_contact contact
    ON contribution.contact_id = contact.id
WHERE
    UNIX_TIMESTAMP( receive_date ) BETWEEN %d AND %d
GROUP BY
    email
EOS;

        $sql = <<<EOS
INSERT INTO {wmf_eoy_receipt_job}
    ( start_time, year )
VALUES
    ( '%s', %d )
EOS;
        db_query( $sql, $job_timestamp, $this->year );

        $sql = <<<EOS
SELECT job_id FROM {wmf_eoy_receipt_job}
    WHERE start_time = '%s'
EOS;
        $result = db_query( $sql, $job_timestamp );
        $row = db_fetch_object( $result );
        $this->job_id = $row->job_id;

        $sql = <<<EOS
INSERT INTO {wmf_eoy_receipt_donor}
  ( job_id, email, name, preferred_language, status, contributions_rollup )
  {$select_query}
EOS;
        db_query( $sql, $this->job_id, $year_start, $year_end );

        $num_rows = db_affected_rows();
        watchdog( 'wmf_eoy_receipt',
            t( "Compiled summaries for !num donors giving during !year",
                array(
                    "!num" => $num_rows,
                    "!year" => $this->year,
                )
            )
        );
    }

    function send_letters()
    {
        $mailer = new Mailer();

        $sql = <<<EOS
SELECT *
FROM {wmf_eoy_receipt_donor}
WHERE
    status = 'queued'
    AND job_id = %d
LIMIT %d
EOS;
        $result = db_query( $sql, $this->job_id, $this->batch_max );
        $succeeded = 0;
        $failed = 0;

        while ( $row = db_fetch_array( $result ) )
        {
            $email = $this->render_letter( $row );

            if ( $this->test ) {
                $email[ 'to_address' ] = variable_get( 'wmf_eoy_test_email', null );
            }

            $success = $mailer->send( $email );

            if ( $success ) {
                $status = 'sent';
                $succeeded += 1;
            } else {
                $status = 'failed';
                $failed += 1;
            }

            $sql = "UPDATE {wmf_eoy_receipt_donor} SET status='%s' WHERE email='%s'";
            db_query( $sql, $status, $row[ 'email' ] );
        }

        watchdog( 'wmf_eoy_receipt',
            t( "Successfully sent !succeeded messages, failed to send !failed messages.",
                array(
                    "!succeeded" => $succeeded,
                    "!failed" => $failed,
                )
            )
        );
    }

    function render_letter( $row ) {
        $language = Translation::normalize_language_code( $row[ 'preferred_language' ] );
        $subject = Translation::get_translated_message( 'donate_interface-email-subject', $language );
        $contributions = array_map(
            function( $contribution ) {
                $terms = explode( ' ', $contribution );
                return array(
                    'date' => $terms[0],
                    'amount' => round( $terms[1], 2 ),
                    'currency' => $terms[2],
                );
            },
            explode( ',', $row[ 'contributions_rollup' ] )
        );
        $total = array_reduce( $contributions,
            function( $sum, $contribution ) {
                return $sum + $contribution[ 'amount' ];
            },
            0
        );

        $template_params = array(
            'name' => 'name',
            'contributions' => $contributions,
            'total' => $total,
        );
        $template = $this->get_template( $language, $template_params );
        $email = array(
            'from_name' => $this->from_name,
            'from_address' => $this->from_address,
            'to_name' => $row[ 'name' ],
            'to_address' => $row[ 'email' ],
            'subject' => $subject,
            'plaintext' => $template->render( 'txt' ),
            'html' => $template->render( 'html' ),
        );

        return $email;
    }

    function get_template( $language, $template_params ) {
        return new Templating(
            self::$templates_dir,
            self::$template_name,
            $language,
            $template_params
        );
    }
}
