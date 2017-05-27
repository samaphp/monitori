<?php

/**
 * Implements hook_cronapi().
 */
function custom_cronapi($op, $job = NULL) {
  // This for the non-trigger cron jobs.
  $items['custom_main_cron'] = array(
    'description' => 'Custom Queue.',
    'rule' => '* * * * *', // Every minute.
    'callback' => 'custom_cron_handler'
  );
  return $items;
}


/**
 * Implements hook_cron_queue_info().
 */
function data_queue_cron_queue_info() {
  $queues = array();
  $queues['custom_queue'] = array(
    'worker callback' => 'custom_queue_worker',
    'time' => 30,
    //'skip on cron' => TRUE
  );
  return $queues;
}

function custom_queue_worker($item){
  $nid = $item->data['nid'];
  $url = $item->data['url'];

  //if (filter_var($url, FILTER_VALIDATE_URL) === TRUE) {
    $options = array();
    //$options['a'] = '';
    $options['headers'] = array(
      'User-Agent' => 'Monitori (+http://monitori.org/)',
    );

    //$start_request = microtime(true);
    //$http_request = drupal_http_request($url, $options);
    //$request_time = microtime(true) - $start_request;
    //$info_array = array();
    //$info_array['time'] = $request_time;
    $http_request = custom_http_get_info($url);
    if($http_request['http_code'] == 200){
      watchdog('custom', 'QUEUE item @url response @response', array('@response'=>$http_request['http_code'], '@url'=>$url) );

      $myLog = array();
      $myLog['type'] = 'check';
      $myLog['operation'] = 'up';
      $myLog['form_id'] = 'custom_run_queue';
      $myLog['description'] = 'up';
      $myLog['ref_numeric'] = $item->data['nid'];
      $myLog['ref_char'] = 'node';
      $myLog['info'] = $info_array;
      event_log_insert($myLog);

      return TRUE;
    }else{
      watchdog('custom', 'QUEUE item @url response @response', array('@response'=>$http_request['http_code'], '@url'=>$url) );
    }
  //}

      $myLog = array();
      $myLog['type'] = 'check';
      $myLog['operation'] = 'down';
      $myLog['form_id'] = 'custom_run_queue';
      $myLog['description'] = 'down';
      $myLog['ref_numeric'] = $item->data['nid'];
      $myLog['ref_char'] = 'node';
      $myLog['info'] = $info_array;
      event_log_insert($myLog);

  watchdog('custom_queue_worker', 'WORKER');
  return FALSE;
}


/**
 * Start reading files.
 */
function custom_run_queue(){
  $customQueue = DrupalQueue::get('custom_queue');
  $countThisProcess = 0;
  $thisProcessTotal = variable_get('custom_process_queue_items_each_time', 100);
  while ($item = $customQueue->claimItem(0)) {
    $countThisProcess++;
    if(custom_queue_worker($item)){
      watchdog('custom', 'SUCCESS item processed. '.print_r($item, TRUE).'');
      $customQueue->deleteItem($item);
/*
      $myLog = array();
      $myLog['type'] = 'check';
      $myLog['operation'] = 'up';
      $myLog['form_id'] = 'custom_run_queue';
      $myLog['description'] = 'up';
      $myLog['ref_numeric'] = $item->data['nid'];
      $myLog['ref_char'] = 'node';
      $myLog['info'] = '';
      event_log_insert($myLog);
*/
    }else{
/*
      $myLog = array();
      $myLog['type'] = 'check';
      $myLog['operation'] = 'down';
      $myLog['form_id'] = 'custom_run_queue';
      $myLog['description'] = 'down';
      $myLog['ref_numeric'] = $item->data['nid'];
      $myLog['ref_char'] = 'node';
      $myLog['info'] = '';
      event_log_insert($myLog);
*/
      // We will delete this item and create it again to be last item in the queue.
      watchdog('custom', 'FAIL item processed. '.print_r($item, TRUE).'');
      $customQueue->deleteItem($item);
      //$customQueue->createItem($item->data);

	global $language;

      // Send email.
      $nodeData = node_load($item->data['nid']);
      $userData = user_load($nodeData->uid);
      $from = 'no-reply@seohover.com';
      $to = $userData->mail;
      $params = array('to' => $to,
                'uid' => $userData->uid,
                'site' => $item->data['url'],
		);
      drupal_mail('custom', 'site_down', $to, $language->language, $params, $from, $send = TRUE);

    }

    // @TODO: We might use release item instead of delete in case of fail.
    if($countThisProcess == $thisProcessTotal){
      break;
    }
  }
}


function custom_cron_handler(){

  watchdog('custom', 'Start cron.');
  $interval = variable_get('custom_cron_interval', 5 * 1);
  if (time() >= variable_get('custom_cron_next_execution', 0)) {
    watchdog('custom', 'Start execution.');

    // We will get all websites and save them in queue.
    $result = db_query("SELECT nid FROM node WHERE type = :nodeType AND status = 1", array(':nodeType'=>'website'));
    $nids = array();
    foreach ($result as $obj) {
      if( isset($obj->nid) ){
        $node = node_load($obj->nid);
        // We might check and validate URL before insert it in queue.

        $customQueue = DrupalQueue::get('custom_queue');
        $queue_item = array('url'=>$node->title, 'nid'=>$node->nid);
        $customQueue->createItem($queue_item);
      }
    }


    // We will run the queue now.
    custom_run_queue();

    variable_set('custom_cron_next_execution', time() + $interval);
  }
  watchdog('custom', 'Finish cron.');

}
