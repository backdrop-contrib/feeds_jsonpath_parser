<?php
// $Id$

/**
 * @file
 *
 * Provides the Class for Feeds JSONPath Parser.
 */

/**
 * Base class for the HTML and XML parsers.
 */
class FeedsJSONPathParser extends FeedsParser {

  /**
   * Implementation of FeedsParser::parse().
   */
  public function parse(FeedsImportBatch $batch, FeedsSource $source) {
    $raw = trim($batch->getRaw());
    $array = json_decode($raw, TRUE);

    if (!is_array($array)) {
      $raw = preg_replace('/}\s*{/', '},{', $raw);
      $raw = '[' . $raw . ']';
      $array = json_decode($raw, TRUE);
    }

    if (is_array($array)) {
      require_once 'jsonpath-0.8.1.php';
      $source_config = $source->getConfigFor($this);
      $all_items = $this->jsonPath($array, $source_config['context']);
      unset($array);

      foreach ($all_items as $item) {
        $parsed_item = array();
        foreach ($source_config['sources'] as $source => $query) {
          $parsed_item[$source] = $this->parseSourceElement($item, $query);
        }
        $batch->addItem($parsed_item);
      }
    }
    else {
      throw new Exception(t('There was an error decoding the JSON document.'));
    }
  }

  /**
   * Utilizes the jsonPath function from jsonpath-0.8.1.php
   *
   * jsonPath returns false if the expression returns zero results and that will
   * mess up our for loops, so return an empty array instead.
   *
   * @todo
   *   Firgure out error handling.
   * @param $array
   *   The input array to parse
   * @$expression
   *   The JSONPath expression.
   * @return array
   *   Returns an array that is the output of jsonPath
   */
  private function jsonPath($array, $expression) {
    $result = jsonPath($array, $expression);
    return ($result === FALSE) ? array() : $result;
  }

  /**
   * Parses one item from the context array.
   *
   * @param $item
   *   A PHP array.
   * @param $query
   *   A JSONPath query.
   * @return array
   *   An array containing the results of the query.
   */
  protected function parseSourceElement($item, $query) {
    if (empty($query)) {
      return;
    }
    $results = $this->jsonPath($item, $query);
    unset($item);

    /**
     * If their is one result, return it directly.  If there is more than one,
     * return the array.
     */
    $count = count($results);
    if ($count == 1) {
      return $results[0];
    }
    if ($count > 1) {
      return $results;
    }
  }

  /**
   * Source form.
   */
  public function sourceForm($source_config) {
    $form = array();
    $form['#weight'] = -10;

    $mappings_ = feeds_importer($this->id)->processor->config['mappings'];
    $uniques = $mappings = array();

    foreach ($mappings_ as $mapping) {
      if (strpos($mapping['source'], 'jsonpath_parser:') === 0) {
        $mappings[$mapping['source']] = $mapping['target'];
        if ($mapping['unique']) {
          $uniques[] = $mapping['target'];
        }
      }
    }

    if (empty($mappings)) {
      $form['error_message']['#value'] = 'FeedsJSONPathParser: No mappings were defined.';
      return $form;
    }

    $form['context'] = array(
      '#type'          => 'textfield',
      '#title'         => t('Context'),
      '#required'      => TRUE,
      '#description'   => t('This is the base query, all other queries will run in this context.'),
      '#default_value' => isset($source_config['context']) ? $source_config['context'] : '',
      '#maxlength'     => 1024,
    );

    $form['sources'] = array(
      '#type' => 'fieldset',
    );

    if (!empty($uniques)) {
      $items = array(
        format_plural(count($uniques),
          t('Field <strong>!column</strong> is mandatory and considered unique: only one item per !column value will be created.',
            array('!column' => implode(', ', $uniques))),
          t('Fields <strong>!columns</strong> are mandatory and values in these columns are considered unique: only one entry per value in one of these columns will be created.',
            array('!columns' => implode(', ', $uniques)))),
      );
      $form['sources']['help']['#value'] = '<div class="help">' . theme('item_list', $items) . '</div>';
    }

    foreach ($mappings as $source => $target) {
      $form['sources'][$source] = array(
        '#type'          => 'textfield',
        '#title'         => $target,
        '#description'   => t('The JSONPath expression to run.'),
        '#default_value' => isset($source_config['sources'][$source]) ? $source_config['sources'][$source] : '',
        '#maxlength'     => 1024,
      );
    }

    return $form;
  }

  /**
  * Override parent::getMappingSources().
  */
  public function getMappingSources() {
    return array(
      'jsonpath_parser:0' => array(
        'name' => t('JSONPath Expression'),
        'description' => t('Allows you to configure an JSONPath expression that will populate this field.'),
      ),
    ) + parent::getMappingSources();
  }

  /**
   * Define defaults.
   */
  public function sourceDefaults() {
    return array(
      'context' => '',
      'sources' => array(),
    );
  }

  /**
   * Override parent::sourceFormValidate().
   *
   * Simply trims all JSONPath values from the form. That way when testing them
   * later we can be sure that there aren't any strings with spaces in them.
   *
   * @param &$values
   *   The values from the form to validate, passed by reference.
   */
  public function sourceFormValidate(&$values) {
    $values['context'] = trim($values['context']);
    foreach ($values['sources'] as &$query) {
        $query = trim($query);
    }
  }
}

/**
 * Implementation of hook_form_feeds_ui_mapping_form_alter().
 *
 * This is an interesting bit of work. Each source name has to be unique,
 * but we have no idea how many to create with getMappingSources() because we
 * don't know how many targets there are going to be.
 *
 * Solution is to keep track in the form how many have been added.
 */
function feeds_jsonpath_parser_form_feeds_ui_mapping_form_alter(&$form, &$form_state) {
  $newest_jsonpath_mapping = array();
  foreach ($form['#mappings'] as $mapping) {
    if (strpos($mapping['source'], 'jsonpath_parser:') === 0) {
      $newest_jsonpath_mapping = $mapping;
    }
  }
  if (!empty($newest_jsonpath_mapping)) {
    list($a, $count) = explode(':', $newest_jsonpath_mapping['source']);
    $default_source = $a . ':' . '0';
    $label = $form['source']['#options'][$default_source];
    unset($form['source']['#options'][$default_source]);
    $form['source']['#options'][$a . ':' . ++$count] = $label;
  }
}