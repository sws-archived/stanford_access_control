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

  $form['rules_list'] = array(
    '#title' => t("Add new path."),
    '#type' => 'fieldset',
    '#prefix' => '<div id="rules_list">',
    '#suffix' => '</div>',
    '#description' => t('Please enter the relative path. When user opens this
      url, they will asked to enter password to view this page. For example,
      "node/5", "new-events", "subsite/*" etc.'),
  );

  $form['rules_list']['path'] = array(
    '#type' => 'textfield',
    '#title' => t('Relative Path'),
    '#description' => t('Enter relative drupal path. For example, "node/5", "new-events", "subsite/*" etc.'),
    '#required' => TRUE,
  );

  $form['rules_list']['submit'] = array(
    '#type' => 'submit',
    '#ajax' => array(
      'callback' => 'stanford_access_control_configure_submit_callback',
      'wrapper' => 'rules_list',
      'effect' => 'fade',
      'method' => 'replace',
      'progress' => array('type' => 'throbber', 'message' => t('Please wait. The data is being saved.')),
    ),
    '#value' => t('Save'),
  );

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
      'data' => t('Relative Path'),
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
        l(t('Edit'), "admin/config/stanford/access_control/" . $data->pid . "/edit"),
        l(t('Delete'), 'admin/config/stanford/access_control/' . $data->pid . "/delete"),
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

  $form['rules_list'] = array(
    '#title' => t("Edit Protected Page Relative path and password."),
    '#type' => 'fieldset',
    '#prefix' => '<div id="rules_list">',
    '#suffix' => '</div>',
    '#description' => t('Please enter the relative path and its corresponding
    password. When user opens this url, they will assked to enter password to
    view this page. For example, "node/5", "new-events" etc.'),
  );

  $form['rules_list']['path'] = array(
    '#type' => 'textfield',
    '#title' => t('Relative Path'),
    '#description' => t('Enter relative drupal path. For example, "node/5", "new-events" etc.'),
    '#required' => TRUE,
    '#default_value' => $protected_page->path,
  );

  $form['rules_list']['enabled'] = array(
    '#type' => 'checkbox',
    '#title' => t('Enable'),
    '#description' => t('Uncheck to disable this protected path.'),
    '#default_value' => $protected_page->enabled,
  );

  $form['rules_list']['pid'] = array(
    '#type' => 'hidden',
    '#value' => $pid,
  );

  $form['rules_list']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save'),
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

  drupal_set_message('The protected page settings has been successfully saved.');
  $form_state['redirect'] = 'admin/config/stanford/access_control';

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
  $cancel = 'admin/config/stanford/access_control';
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

  // Let the user know stuff happened.
  drupal_set_message(t('The path has been successfully deleted from the protected pages.'));

  // Got to go somewhere...
  $form_state['redirect'] = 'admin/config/stanford/access_control';
}

/**
 * Callback function for protected pages settings.
 */
function stanford_access_control_settings() {

  $form['stanford_access_control_password_fieldset'] = array(
    '#type' => 'fieldset',
    '#title' => t('Stanford Access Control Password Settings'),
    '#description' => t('Configure password related settings.'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
  );

  // Password Field.
  $form['stanford_access_control_password_fieldset']['stanford_access_control_global_password_field'] = array(
    '#type' => 'password_confirm',
    '#title' => t('Password'),
    '#description' => t('The default password for all protected pages.'),
  );

  // $form['stanford_access_control_password_fieldset']['stanford_access_control_session_expire_time'] = array(
  //   '#type' => 'textfield',
  //   '#title' => t('Session Expire Time'),
  //   '#description' => t('When user enters password a session is created.
  //     The node will be accessible until session expire. Once session expires,
  //     user will need to enter password again. The default session expire time
  //     is 0 (unlimited).'),
  //   '#default_value' => variable_get('stanford_access_control_session_expire_time', 0),
  //   '#required' => TRUE,
  //   '#size' => 10,
  //   '#element_validate' => array('stanford_access_control_validate_integer_positive'),
  //   '#field_suffix' => t('in minutes'),
  // );

  $form['stanford_access_control_other_fieldset'] = array(
    '#type' => 'fieldset',
    '#title' => t('Protected Page Settings'),
    '#description' => t('Configure password prompt page.'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
  );

  // Page title.
  $form['stanford_access_control_other_fieldset']['stanford_access_control_title'] = array(
    '#type' => 'textfield',
    '#title' => t('Password page title'),
    '#default_value' => variable_get('stanford_access_control_title', t('Protected Page -- Enter password')),
    '#description' => t('Enter the title of the protected page.'),
  );

  // Description text.
  $form['stanford_access_control_other_fieldset']['stanford_access_control_description'] = array(
    '#type' => 'textarea',
    '#title' => t('Password page description (inside the field set)'),
    '#default_value' => variable_get('stanford_access_control_description', t('The page you are trying to view is password protected. Please enter the password below to proceed.')),
    '#description' => t('Enter specific description for the protected page. This description is displayed inside the fieldset. HTML is accepted.'),
  );

  // Password field label.
  $form['stanford_access_control_other_fieldset']['stanford_access_control_password_label'] = array(
    '#type' => 'textfield',
    '#title' => t('Password field label'),
    '#default_value' => variable_get('stanford_access_control_password_label', t('Enter Password')),
    '#description' => t('Enter the text for the password field label.'),
  );

  // Submit button text.
  $form['stanford_access_control_other_fieldset']['stanford_access_control_submit_button_text'] = array(
    '#type' => 'textfield',
    '#title' => t('Submit Button Text'),
    '#default_value' => variable_get('stanford_access_control_submit_button_text', t('Authenticate')),
    '#description' => t('Enter the text for the submit button of enter password form.'),
  );

  // Error message text.
  $form['stanford_access_control_other_fieldset']['stanford_access_control_incorrect_password_msg'] = array(
    '#type' => 'textarea',
    '#title' => t('Incorrect Password Error Text'),
    '#default_value' => variable_get('stanford_access_control_incorrect_password_msg', t('Incorrect password!')),
    '#description' => t('This error text will appear if someone enters wrong password in "Enter password screen".'),
  );

  // Add a submit handler.
  $form['#submit'][] = 'stanford_access_control_settings_submit';

  // System!
  return system_settings_form($form);
}

/**
 * Callback function to validate session expire time value.
 */
// function stanford_access_control_validate_integer_positive($element, &$form_state) {
//   $value = $element['#value'];
//   if ($value !== '' && (!is_numeric($value) || intval($value) != $value || $value < 0)) {
//     form_error($element, t('%name must be a positive integer.', array('%name' => $element['#title'])));
//   }
// }

/**
 * Custom submit function encrypt password and deleting non-useful variable.
 */
function stanford_access_control_settings_submit($form, &$form_state) {

  $passwd = $form_state['values']['stanford_access_control_global_password_field'];

  if ($passwd) {
    variable_set('stanford_access_control_global_password', sha1($passwd));
  }

}


