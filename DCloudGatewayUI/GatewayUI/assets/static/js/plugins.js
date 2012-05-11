function field_msg_ctrl(obj, stat, msg) {
	var target = $("label[for=" + obj.name + "]");
	target.find("span.label").remove();
	if (stat && msg) {
		target.append('<span>' + msg + "</span>");
		target.find("span").addClass("label label-" + stat).css("margin-left", "5px");
	}
}


jQuery.extend({
	alert: function(type, title, msg) {
		$("div.alert").remove();
		var alert_div = $('<div class="alert alert-' + type + ' fade in"><a class="close" href="#">×</a></div>').addClass(type);
		alert_div.append($("<p><h4 class='alert-heading'>" + title + "</h4>" + msg + "</p>"));
		$("body > .container > .content").prepend(alert_div);
	},
	getCookie: function(name) {
		var cookieValue = null;
		if (document.cookie && document.cookie != '') {
			var cookies = document.cookie.split(';');
			var i;
			for (i = 0; i < cookies.length; i += 1) {
				var cookie = jQuery.trim(cookies[i]);
				// Does this cookie string begin with the name we want?
				if (cookie.substring(0, name.length + 1) == (name + '=')) {
					cookieValue = decodeURIComponent(cookie.substring(name.length + 1));
					break;
				}
			}
		}
		return cookieValue;
	}
});

jQuery(document).ajaxSend(function(event, xhr, settings) {
    function sameOrigin(url) {
        // url could be relative or scheme relative or absolute
        var host = document.location.host; // host + port
        var protocol = document.location.protocol;
        var sr_origin = '//' + host;
        var origin = protocol + sr_origin;
        // Allow absolute or scheme relative URLs to same origin
        return (url == origin || url.slice(0, origin.length + 1) == origin + '/') ||
            (url == sr_origin || url.slice(0, sr_origin.length + 1) == sr_origin + '/') ||
            // or any other URL that isn't scheme relative or absolute i.e relative.
            !(/^(\/\/|http:|https:)\.*/.test(url));
    }
    function safeMethod(method) {
        return (/^(GET|HEAD|OPTIONS|TRACE)$/.test(method));
    }

    if (!safeMethod(settings.type) && sameOrigin(settings.url)) {
        xhr.setRequestHeader("X-CSRFToken", $.getCookie('csrftoken'));
    }
});

function field_check(obj) {

	switch (obj.type) {
	case "radio":
	case "checkbox":
		break;
	default:
		field_msg_ctrl(obj);
	}

	if (obj.getAttribute("valid.required")) {
		if (["radio", "checkbox"].indexOf(obj.type) != -1) {
			if (!$("input[name=" + obj.name + "]").is(":checked")) {
				field_msg_ctrl(obj, "important", "required");
				return obj;
			} else {
				field_msg_ctrl(obj);
			}
		} else if (!$(obj).val()) {
			field_msg_ctrl(obj, "important", "required");
			return obj;
		}
	}

	pattern = me.getAttribute("valid.regex");
	if (pattern) {
		regRule = new RegExp(pattern);
		if (!$(obj).val().match(regRule)) {
			field_msg_ctrl(obj, "important", "invalid value");
			return obj;
		}
	}

	if (obj.getAttribute("valid.min") || obj.getAttribute("valid.max")) {

		min = obj.getAttribute("valid.min");
		max = obj.getAttribute("valid.max");

		switch (obj.type) {
		case "text":
		case "password":
			obj.len = $(obj).val().length;
			msg = "invalid length";
			break;
		case "number":
			obj.len = $(obj).val();
			msg = "invalid value";
			break;
		case 'checkbox':
			obj.len = $("input[name=" + me.name + "]").filter(":checked").length;
			msg = "invalid selection";
			break;
		}

		if (obj.len < min || obj.len > max) {
			field_msg_ctrl(obj, "important", msg);
			return obj;
		}
	}

	password_id = obj.getAttribute("valid.password");
	if (password_id && $(obj).val() != $("#" + password_id).val()) {
		field_msg_ctrl(obj, "important", "passwords not match");
		return obj;
	}
}

jQuery.fn.extend({
	getValues: function(dataType) {
		var params = {};
		this.each(function() {
			tag = this.tagName.toLowerCase();
			if (tag == 'form') {
				params = $(':input', this).getValues();
			} else {
				var name = $(this).attr('name');
				switch (this.type) {
				case 'text':
				case 'textarea':
				case 'number':
				case 'password':
				case 'select-multiple':
				case 'select-one':
				case 'hidden':
					params[name] = $(this).val();
					break;
				case 'checkbox':
					if (!params[name]) {
						params[name] = [];
						$(":input[name=" + name + "]:checked").each(function() {
							params[name].push($(this).val());
						});
					}
					break;
				case 'radio':
					if (!params[name]) {
						params[name] = $(":input[name=" + name + "]:checked").val() || params[name];
					}
					break;
				}
			}
		});
		if (dataType == "json") {
			return $.toJSON(params);
		} else {
			return params;
		}
	},

	validator: function() {
		error_input = $();
		this.each(function() {
			tag = this.tagName.toLowerCase();
			if (tag == 'form') {
				return $(':input', this).validator();
			} else {
				me = this;
				switch (me.type) {
				case 'text':
				case 'password':
				case 'number':
				case 'textarea':
				case 'select-multiple':
				case 'select-one':
				case 'checkbox':
				case 'radio':
					if (field_check(me)) {
						error_input.push(field_check(me));
					}
					break;
				}
			}
		});
		error_input.first().focus();
		return error_input;
	}
});

jQuery.fn.sortElements = (function(){
    var sort = [].sort;

    return function(comparator, getSortable) {
        getSortable = getSortable || function(){return this;};

        var placements = this.map(function(){
            var sortElement = getSortable.call(this),
                parentNode = sortElement.parentNode,
                // Since the element itself will change position, we have
                // to have some way of storing its original position in
                // the DOM. The easiest way is to have a 'flag' node:
                nextSibling = parentNode.insertBefore(
                    document.createTextNode(''),
                    sortElement.nextSibling
                );
            return function() {
                if (parentNode === this) {
                    throw new Error(
                        "You can't sort elements if any one is a descendant of another."
                    );
                }
                // Insert before flag:
                parentNode.insertBefore(this, nextSibling);
                // Remove flag:
                parentNode.removeChild(nextSibling);
            };
        });
        return sort.call(this, comparator).each(function(i){
            placements[i].call(getSortable.call(this));
        });
    };
})();

$(function(){
	$('.btn-group[data-toggle="buttons-radio"]').on('click', 'button', function(e){
		e.preventDefault();
		target = $(this);
		target.parent().find(".btn-primary").removeClass("btn-primary")
		.find("input").attr("checked", false);
		target.addClass("btn-primary")
		.find("input").attr("checked", true);
	});
});
