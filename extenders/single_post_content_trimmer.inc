<?php
/**
 * Posts extender: trim contents to force the user to go to the source URL.
 * TOP PRIORITY
 * 
 * @package    vsimporter
 * @author     Alejandro Caballero - lava.caballero@gmail.com
 */

global $config, $settings, $post, $template;

if( empty($post) ) return;

$source = $post->get_meta("vsimporter.source_url");

if( empty($source) ) return;

if( $template->get("page_tag") != "single_post" )
{
    if( ! empty($source) )
        $config->globals["processing_contents"]
            .= "<p><i>{$this_module->language->source} <a href='$source' target='_blank'>$source</a></i></p>\n";
    
    return;
}

$maxlength = (int) $settings->get("modules:vsimporter.trim_contents");
if( empty($maxlength) )
{
    if( ! empty($source) )
        $config->globals["processing_contents"]
            .= "<p><i>{$this_module->language->source} <a href='$source' target='_blank'>$source</a></i></p>\n";
    
    return;
}

$contents = $config->globals["processing_contents"];

if( strlen($contents) < $maxlength )
{
    if( ! empty($source) )
        $config->globals["processing_contents"]
            .= "<p><i>{$this_module->language->source} <a href='$source' target='_blank'>$source</a></i></p>\n";
    
    return;
}

$contents = substr($contents, 0, $maxlength);

if( substr($contents, -1) == "\n" )
{
    $config->globals["processing_contents"] = $contents;
    
    if( ! empty($source) )
        $config->globals["processing_contents"]
            .= "<p><i>{$this_module->language->source} <a href='$source' target='_blank'>$source</a></i></p>\n";
    
    return;
}

$pos = strrpos($contents, "\n");

if( $pos === false || $pos < 0 )
{
    $config->globals["processing_contents"] = $contents;
    
    if( ! empty($source) )
        $config->globals["processing_contents"]
            .= "<p><i>{$this_module->language->source} <a href='$source' target='_blank'>$source</a></i></p>\n";
    
    return;
}

$contents = substr($contents, 0, $pos);
if( empty($source) )
{
    $config->globals["processing_contents"] = $contents;
    
    return;
}

$contents .= "<p><i>{$this_module->language->source} <a href='$source' target='_blank'>$source</a></i></p>\n";
$contents .= "<p style='font-size: 1.2em; line-height: 1.4em;'><a href='$source' target='_blank'>{$this_module->language->continue_reading}</a></p>\n";
$config->globals["processing_contents"] = $contents;
