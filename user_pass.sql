-- 建立yourdb库
create database login_info;

-- 创建user表
USE login_info;

-- 表名为user
CREATE TABLE user(
    username char(50) NULL,
    password char(50) NULL
)ENGINE=InnoDB;

-- 添加用户名-密码信息
INSERT INTO user(username, password) VALUES('ping', 'ping');