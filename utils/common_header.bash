# Enable debug only if verbose on
if [ "$verbose" = "1" ]; then set -x; else set +x; fi
source $WORKSPACE/utils/trace_error.bash
set -e

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
		if [ -n "$setup_dev_env_mode" ]; then
			echo "Require packages for mode $setup_dev_env_mode: $install"
		fi
		sudo apt-get update
		sudo apt-get install -y $install $force_install
		packages=""
		install=""
		force_install=""
	fi
	eval set $flag_x
}
