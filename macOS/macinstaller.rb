#!/usr/bin/env ruby
# -*- coding: utf-8 -*-

# based on https://github.com/opentoonz/opentoonz_installer_osx

require 'FileUtils'
if ARGV.size != 2 then
  puts "usage: ./app.rb [SRC_BUNDLE_PATH] [VERSION(float)]"
  exit 1
end

def exec_with_assert(cmd)
    result = `#{cmd}`
    if $? != 0 then
        puts "Execution '#{cmd}' failed."
        exit 1
    end
    puts "Execution '#{cmd}' succeed."
end

# 定数群
SRC_BUNDLE_PATH = ARGV[0][-1] == "/" ? ARGV[0][0, ARGV[0].size-1] : ARGV[0]
VERSION = ARGV[1]
#DELETE_RPATH = ARGV[2]
VIRTUAL_ROOT = "VirtualRoot"
INSTALL_BUNDLE = "KumoWorks.app"
APP = "Applications"
THIS_DIRECTORY = File.dirname(__FILE__)
RESOURCE_DIR = "ResourceTmp"

PKG_ID = "io.github.opentoonz"
PKG_TMP = "KumoWorksBuild.pkg"
FINAL_PKG = "KumoWorks.pkg"

# VirtualRoot への設置
# 既存を削除して設置
if File.exist? VIRTUAL_ROOT then
    exec_with_assert "rm -rf #{VIRTUAL_ROOT}"
end
exec_with_assert "mkdir -p #{VIRTUAL_ROOT}/#{APP}"
exec_with_assert "cp -r #{SRC_BUNDLE_PATH} #{VIRTUAL_ROOT}/#{APP}/#{INSTALL_BUNDLE}"

# plist が存在しない場合は生成
PKG_PLIST = "app.plist"
unless File.exist? PKG_PLIST then
    exec_with_assert "pkgbuild --root #{VIRTUAL_ROOT} --analyze #{PKG_PLIST}"
end

# ライセンス
if File.exist? RESOURCE_DIR then
    exec_with_assert "rm -r #{RESOURCE_DIR}"
end
exec_with_assert "mkdir #{RESOURCE_DIR}"

unless File.exists? "#{RESOURCE_DIR}/Japanese.lproj"
    FileUtils.cp_r("#{THIS_DIRECTORY}/Japanese.lproj", "#{RESOURCE_DIR}")
end
unless File.exists? "#{RESOURCE_DIR}/English.lproj" 
    FileUtils.cp_r("#{THIS_DIRECTORY}/English.lproj", "#{RESOURCE_DIR}")
end
# plist を用いた pkg の生成
exec_with_assert "pkgbuild --root #{VIRTUAL_ROOT} --component-plist #{PKG_PLIST} --identifier #{PKG_ID} --version #{VERSION} #{PKG_TMP}"

# distribution.xml が存在しない場合は生成
DIST_XML = "#{RESOURCE_DIR}/distribution.xml"
unless File.exists? DIST_XML then
    exec_with_assert "productbuild --synthesize --package #{PKG_TMP} #{DIST_XML}"
    exec_with_assert "gsed -i -e \"3i <title>KumoWorks</title>\" #{DIST_XML}"
    exec_with_assert "gsed -i -e \"6i <license file='License.rtf'></license>\" #{DIST_XML}"
end

# 最終的な pkg を生成
exec_with_assert "productbuild --distribution #{DIST_XML} --package-path #{PKG_TMP} --resources #{RESOURCE_DIR} #{FINAL_PKG}"

# 一時的な生成物を削除
`rm #{PKG_TMP}`
`rm -r #{RESOURCE_DIR}`
`rm -r #{VIRTUAL_ROOT}`
`rm app.plist`
