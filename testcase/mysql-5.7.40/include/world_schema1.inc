CREATE TABLE country (
  Code char(3) NOT NULL default '',
  Name char(52) NOT NULL default '',
  SurfaceArea float(10,2) NOT NULL default '0.00',
  Population int(11) NOT NULL default '0',
  Capital int(11) default NULL
);
CREATE TABLE city (
  ID int(11) NOT NULL,
  Name char(35) NOT NULL default '',
  Country char(3) NOT NULL default '',
  Population int(11) NOT NULL default '0'
);
# Myisam don't return error when out of range, should modify the range of Percentage
CREATE TABLE countrylanguage (
  Country char(3) NOT NULL default '',
  Language char(30) NOT NULL default '',
#  Percentage float(3,1) NOT NULL default '0.0'
  Percentage float(4,1) NOT NULL default '0.0'
);
