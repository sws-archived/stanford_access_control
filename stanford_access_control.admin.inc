<?php
/**
 * @file
 *
 * The majority of this code was taken from and modified from the
 * Protected Pages Drupal project. https://www.drupal.org/project/stanford_access_control
 *
 */


/**
 * Callback function for add protected page.
 */
function stanford_access_control_configure($form, &$form_state) {

  $form["rules_ops"]["create_new"]["#markup"] = "<p>" . l("+ Create new protected path", "admin/config/stanford/access-control/new", array("attributes" => array("class" => "btn"))) . "</p>";

  $form['rules_list']['pages_table'] = array(
    '#markup' => stanford_access_control_get_pages_list(),
  );

  return $form;
}


/**
 * Callback to generate list of protected pages.
 */
function stanford_access_control_get_pages_list() {

  $header = array(
    array(
      'data' => t('#'),
    ),
    array(
      'data' => t('Protected Path(s)'),
    ),
    array(
      'data' => t('Enabled / Disabled'),
    ),
    array(
      'data' => t('Operations'),
      'colspan' => 2,
    ),
  );

  $rows = array();
  $query = db_select('stanford_access_control', 'sac')
    ->extend('PagerDefault')
    ->extend('TableSort');

  $query->fields('sac')
    ->limit(20)
    ->orderByHeader($header);

  $result = $query->execute();

  $rows = array();
  $count = 1;
  foreach ($result as $data) {
    $rows[] = array(
      'data' =>
      array(
        $count,
        $data->path,
        ($data->enabled) ? t("Enabled") : t("Disabled"),
        l(t('Edit'), "admin/config/stanford/access-control/" . $data->pid . "/edit"),
        l(t('Delete'), 'admin/config/stanford/access-control/' . $data->pid . "/delete"),
      ),
    );
    $count++;
  }

  $output = theme('table',
    array(
      'header' => $header,
      'rows' => $rows,
      "sticky" => TRUE,
      "caption" => "",
      "colgroups" => array(),
      "empty" => t("No record found!"),
    )
  );

  $output .= theme('pager', array('tags' => array()));
  return $output;
}

/**
 * [stanford_access_control_create_new_rule_form description]
 * @param  [type] $form        [description]
 * @param  [type] &$form_state [description]
 * @return [type]              [description]
 */
function stanford_access_control_create_new_rule_form($form, &$form_state) {

  stanford_access_control_get_rule_form($form, $form_state);

  return $form;
}

/**
 * Alters the form to add the rule form common elements.
 * @param  [type] &$form       [description]
 * @param  [type] &$form_state [description]
 * @return [type]              [description]
 */
function stanford_access_control_get_rule_form(&$form, &$form_state) {

  global $user;
  $role_opts = user_roles();
  $role_opts_keyed = array_combine($role_opts, $role_opts);

  $form['rules_list'] = array(
    '#title' => t("Protected Page Relative path and password."),
    '#type' => 'fieldset',
    '#prefix' => '<div id="rules_list">',
    '#suffix' => '</div>',
  );

  $form['rules_list']['enabled'] = array(
    '#type' => 'checkbox',
    '#title' => t('Enable'),
    '#description' => t('Uncheck to disable this protected path.'),
    '#default_value' => 1,
  );

  $form['rules_list']['path'] = array(
    '#type' => 'textfield',
    '#title' => t('Relative Path'),
    '#description' => t('Please enter a relative path. This can be the path alias eg: "about-us" or the raw path eg: "node/115" as well as wildcard paths eg: academics*.'),
    '#required' => TRUE,
  );

  $form['rules_list']['roles'] = array(
    '#type' => 'checkboxes',
    '#title' => 'Roles that can view',
    '#description' => t("The roles selected here will be able to view the pages protected by this path rule"),
    '#options' => $role_opts_keyed,
    '#default_value' => isset($user->roles) ? $user->roles : "",
  );

  $form['rules_list']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save'),
  );
}


/**
 * Implements hook_validate().
 */
function stanford_access_control_configure_validate($form, &$form_state) {

  $path = "";
  $raw_value = $form_state['values']['path'];

  $patterns = array(
    '/(\r\n?|\n)/', // newlines
    '/\*/', // asterisks
    '/(^|\|)\<front\>($|\|)/' // <front>
  );

  // Check for pattern first.
  foreach ($patterns as $pattern) {
    if (preg_match($pattern, $raw_value)) {
      $path = $raw_value;
      break;
    }
  }

  // Validate normal path.
  if (empty($path)) {
    $form_state['normal_path'] = drupal_get_normal_path($form_state['values']['path']);
    $path = drupal_strtolower(drupal_get_path_alias($form_state['values']['path']));
    if (!drupal_valid_path($form_state['normal_path'])) {
      form_set_error('path', t('Please enter a correct path!'));
    }
  }

  $pid = db_select('stanford_access_control')
      ->fields('stanford_access_control', array('pid'))
      ->condition(db_or()->condition('path', $form_state['normal_path'])->condition('path', $path))
      ->range(0, 1)
      ->execute()
      ->fetchField();

  if ($pid) {
    form_set_error('path', t('Duplicate path entry is not allowed. There is already a path or its alias exists.'));
  }
}

/**
 * Ajax submit callback for add protected page form.
 */
function stanford_access_control_configure_submit_callback($form, &$form_state) {
  $errors = form_get_errors();

  if (count($errors) < 1) {

    $data = array(
      'password' => sha1(trim(check_plain($form_state['values']['password']))),
      'path' => check_plain($form_state['values']['path']),
    );

    drupal_write_record("stanford_access_control", $data);

    drupal_set_message('The settings has been successfully saved.');
    $form['rules_list']['pages_table']['#markup'] = stanford_access_control_get_pages_list();
    $form['rules_list']['path']['#value'] = '';
  }

  // Must rebuild node_access table...
  node_access_rebuild();

  return $form['rules_list'];
}

/**
 * Callback function for edit protected page form.
 */
function stanford_access_control_edit($form, &$form_state, $pid) {

  if (!is_numeric($pid)) {
    drupal_set_message("PID must be numeric", "error");
    drupal_access_denied();
    exit;
  }

  // Get some data.
  $protected_page = stanford_access_control_get_config_array($pid);

  if (!isset($protected_page->path)) {
    drupal_set_message("No path found for that PID", "error");
    drupal_access_denied();
    exit;
  }

  // Get the shared form to start.
  stanford_access_control_get_rule_form($form, $form_state);

  // Alter a few elements.
  $form['rules_list']['path']["#default_value"] = $protected_page->path;
  $form['rules_list']['enabled']["#default_value"] = $protected_page->enabled;

  // Add the pid.
  $form['rules_list']['pid'] = array(
    '#type' => 'hidden',
    '#value' => $pid,
  );

  return $form;
}

/**
 * Implements hook_validate().
 */
function stanford_access_control_edit_validate($form, &$form_state) {

  $normal_path = drupal_get_normal_path($form_state['values']['path']);
  $path = $form_state['values']['path'];

  $query = db_select('stanford_access_control')
      ->fields('stanford_access_control', array('pid'))
      ->condition('pid', $form_state['values']['pid'], '<>')
      ->range(0, 1);

  if ($normal_path) {
    $query->condition(
      db_or()
        ->condition('path', $normal_path)
        ->condition('path', $path)
    );
  }
  else {
    $query->condition("path", $path);
  }

  $pid = $query->execute()->fetchField();

  if ($pid) {
    form_set_error('path', t('Duplicate path entry is not allowed. There is already a path or its alias exists.'));
  }

}

/**
 * Implements hook_submit().
 */
function stanford_access_control_edit_submit($form, &$form_state) {

  $values = $form_state["values"];
  $values['path'] = check_plain($form_state['values']['path']);

  $data = array(
    "pid" => $values["pid"],
    "path" => $values["path"],
    "enabled" => $values["enabled"],
  );

  $keys = array("pid");

  drupal_write_record("stanford_access_control", $data, $keys);

  // Must rebuild node_access table...
  node_access_rebuild();

  drupal_set_message('The protected page settings has been successfully saved.');
  $form_state['redirect'] = 'admin/config/stanford/access-control';

}

/**
 * Callback function for delete protected page.
 */
function stanford_access_control_delete_confirm($form, &$form_state, $pid) {

  if (!is_numeric($pid)) {
    throw new Exception("PID must be numeric.");
  }

  $data = stanford_access_control_get_config_array($pid);
  $path = $data->path;

  if (empty($path)) {
    throw new Exception("No path found for pid: $pid");
  }

  $form['pid'] = array(
    '#type' => 'hidden',
    '#value' => $pid,
  );

  $message = t('Are you sure you want to delete <b>"%path"</b> from protected pages list?', array('%path' => $path));
  $cancel = 'admin/config/stanford/access-control';
  $description = t('This action cannot be undone.');

  return confirm_form($form, $message, $cancel, $description, t('Delete'), t('Cancel'));
}

/**
 * Implements hook_submit().
 */
function stanford_access_control_delete_confirm_submit($form, &$form_state) {

  // Die if not confirmed.
  if (!$form_state['values']['confirm']) {
    return;
  }

  // Pid from the form submission.
  $pid = $form_state['values']['pid'];

  // Do the delete.
  db_delete('stanford_access_control')
    ->condition('pid', $pid)
    ->execute();

  // Must rebuild node_access table...
  node_access_rebuild();

  // Let the user know stuff happened.
  drupal_set_message(t('The path has been successfully deleted from the protected pages.'));

  // Got to go somewhere...
  $form_state['redirect'] = 'admin/config/stanford/access-control';
}

/**
 * Callback function for protected pages settings.
 */
function stanford_access_control_settings() {

  $form["sac_messages"] = array(
    '#type' => "checkbox",
    '#title' => "Enable protected messages",
    '#description' => "Check this box to show messages on pages that are protected",
    '#default_value' => variable_get("stanford_access_control_messages", TRUE),
  );

  // Add a submit handler.
  $form['#submit'][] = 'stanford_access_control_settings_submit';

  // System!
  return system_settings_form($form);
}

/**
 * Custom submit function encrypt password and deleting non-useful variable.
 */
function stanford_access_control_settings_submit($form, &$form_state) {

  $values = $form_state["values"];
  if ($values["sac_messages"]) {
    variable_set("stanford_access_control_messages", TRUE);
  }
  else {
    variable_set("stanford_access_control_messages", FALSE);
  }

}


