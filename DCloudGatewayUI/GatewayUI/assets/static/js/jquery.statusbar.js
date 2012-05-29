/**
 * jquery.statusbar v1.0
 * copyright 2011 Rob Stortelers (Merula Softworks) - www.merulasoft.nl
 */
(function($){
 $.statusbar = function(options) {

    var defaults = {
		onClick:function(){} //on message click
	};

	var options = $.extend(defaults, options);

	var slided = false ; //is the bar slidedown?

	createBar();

	function createBar() //creates the html for the bar
	{
		//create the bar
		var statusbar = $("<div style=\"z-index: 2;position:relative; width:100%\" class=\"ui-widget statusbar-wrapper\"><div style=\"overflow:auto;\" class=\"ui-widget-content statusbar-content\"></div><div class=\"ui-widget-header statusbar-header\">Error Message<div class=\"toggle-btn\"></div></div></div>");
		$('#errormsgbox').prepend(statusbar);
		// $('html').css("margin-top","26px");
		$('.statusbar-content').hide();
		// var btnRemoveAll = $('<button style="margin:5px;" class="statusbar-content-button-remove-all">Remove all</button>').button().click(removeAll);
		// $('.statusbar-content').append(btnRemoveAll);
		$('.statusbar-header').hover(function(){$(this).toggleClass('ui-state-hover');});
		$('.statusbar-header').click(slide);

		//create text message area
		$('.statusbar-header').append("<div style=\"float:left;margin:5px;\" class=\"statusbar-header-text\">&nbsp;</div>");
	}

	function slide()//show history of messages
	{
		if(!slided)
		{
			$('.tab-content').hide();
			var slide_height = $(".main-row").height() - 70 -$('.statusbar-header').outerHeight()-($('.statusbar-content').outerHeight()-$('.statusbar-content').height());
			if (navigator.appName == 'Microsoft Internet Explorer'){
				slide_height -= 132;
			}
			$('.statusbar-content').height(slide_height);
			$('.statusbar-content').slideDown(function(){
				$(".toggle-btn").toggleClass("active");
			});
		}else
		{
			$('.statusbar-content').slideUp(function(){
				$(".toggle-btn").toggleClass("active");
				$('.tab-content').show(1);
			});
		}
		slided = !slided;
	}

	// function removeAll()
	// {
	// 	$('.statusbar-content-item').remove();
	// }

	// this.addMessage = function(message,argument) //add a message to the bar
	// {
	// 	$('.statusbar-header-text').clearQueue().show().css("opacity","").fadeOut(function()
	// 	{
	// 		$('.statusbar-header-text').text(message).fadeIn();
	// 		addDetails(message,argument);

	// 	});
	// }

	// function addDetails(message,argument) //add to message details
	// {
	// 	var item = $("<div style=\"margin:5px;padding:20px;cursor:pointer;\" class=\"statusbar-content-item ui-widget-content\"></div>");
	// 	item.hover(function(){$(this).toggleClass('ui-state-highlight')})
	// 	item.text(message);
	// 	var btnDelete = $('<button class="statusbar-content-item-button">Remove</button>');
	// 	btnDelete.css('position','relative');
	// 	btnDelete.css('float','right');
	// 	btnDelete.button();
	// 	item.append(btnDelete);
	// 	btnDelete.click(function(){
	// 		item.remove();
	// 	})
	// 	item.click(function(){options.onClick(argument)})
	// 	$('.statusbar-content>button').after(item);
	// }

	return this;
 };
})(jQuery);