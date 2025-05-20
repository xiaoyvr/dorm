{ pkgs, lib, config, inputs, ... }:

{

  # https://devenv.sh/packages/
  #
  languages.cplusplus.enable = true;

  packages = [
    pkgs.vcpkg
    pkgs.cmake
  ];

  # See full reference at https://devenv.sh/reference/options/
}
