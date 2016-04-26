set +x
export TERM=xterm-256color
script_error_report() {
	set +x
	local script="$1"
	local parent_lineno="$2"
	local message="$3"
	local code="${4:-1}"
	echo "Error near ${script} line ${parent_lineno}; exiting with status ${code}"
	if [[ -n "$message" ]] ; then
		echo -e "Message: ${message}"
	fi
	exit "${code}"
}

function install_pkg (){
	[[ "$-" =~ "x" ]] && flag_x="-x" || flag_x="+x"
	set +x
	for pkg in $packages;
	do
		if ! dpkg -s $pkg >/dev/null 2>&1; then
			install="$install $pkg"
		fi
	done
	install="$(echo -e "$install $force_install" | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//')"
	if [ -n "$install" ]; then
		echo -e "\nError: Require packages: $install"
		if [ "$1" = "check_pkg" ]; then
			return 1
		fi
		sudo apt-get update || :
		sudo apt-get install -y $install $force_install
		packages=""
		install=""
		force_install=""
	fi
	eval set $flag_x
}

# Main source
# Enable error trace
trap 'script_error_report "${BASH_SOURCE[0]}" ${LINENO}' ERR
set -e -o errtrace
# Enable debug log only if verbose on
if ${CI_VERBOSE:-false}; then set -x; else set +x; fi
