/**
 * DeepDetect
 * Copyright (c) 2015 Emmanuel Benazera
 * Author: Emmanuel Benazera <beniz@droidnik.fr>
 *
 * This file is part of deepdetect.
 *
 * deepdetect is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * deepdetect is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with deepdetect.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CSVINPUTFILECONN_H
#define CSVINPUTFILECONN_H

#include "inputconnectorstrategy.h"
//#include "ext/csv.h"
#include <fstream>
#include <unordered_set>

namespace dd
{
  class CSVInputFileConn : public InputConnectorStrategy
  {
  public:
    CSVInputFileConn()
      :InputConnectorStrategy() {}
    
    ~CSVInputFileConn() {}
  
    void init(const APIData &ad)
    {
      fillup_parameters(ad,_csv_fname);
    }
    
    void fillup_parameters(const APIData &ad,
			   std::string &fname)
    {
    }
    
    int transform(const APIData &ad)
    {
      APIData ad_input = ad.getobj("parameters").getobj("input");
      if (ad_input.has("filename"))
	_csv_fname = ad_input.get("filename").get<std::string>();
      else throw InputConnectorBadParamException("no CSV filename");
      if (ad_input.has("test_filename"))
	_csv_test_fname = ad_input.get("test_filename").get<std::string>();
      if (ad_input.has("label"))
	_label = ad_input.get("label").get<std::string>();
      //else throw InputConnectorBadParamException("missing label column parameter"); //TODO: should throw only at training in lower sublayers...
      if (ad_input.has("ignore"))
	{
	  std::vector<std::string> vignore = ad_input.get("ignore").get<std::vector<std::string>>();
	  for (std::string s: vignore)
	    _ignored_columns.insert(s);
	}
      if (ad_input.has("id"))
	_id = ad_input.get("id").get<std::string>();
      read_csv(ad_input,_csv_fname);
      // data should already be loaded.
      return 0;
    }

    void read_csv_line(const std::string &hline,
		       const std::string &delim,
		       std::vector<double> &vals,
		       std::string &column_id,
		       int &nlines)
    {
      std::string col;
      auto hit = _ignored_columns.begin();
      std::stringstream sh(hline);
      int c = -1;
      while(std::getline(sh,col,delim[0]))
	{
	  ++c;
	  
	  // detect strings by looking for characters and for quotes
	  // convert to float unless it is string (ignore strings, aka categorical fields, for now)
	  if ((hit=_ignored_columns.find(_columns.at(c)))!=_ignored_columns.end())
	    {
	      std::cout << "ignoring column: " << col << std::endl;
	      continue;
	    }
	  if (_columns.at(c) == _id)
	    {
	      column_id = col;
	      //std::cout << "id column detected: " << col << std::endl;
	      //continue;
	    }
	  try
	    {
	      double val = std::stod(col);
	      vals.push_back(val);
	    }
	  catch (std::invalid_argument &e)
	    {
	      // not a number, skip for now
	      std::cout << "not a number: " << col << std::endl;
	    }
	}
      /*if (!_id.empty())
	_csvdata.insert(std::pair<std::string,std::vector<double>>(column_id,vals));
	else _csvdata.insert(std::pair<std::string,std::vector<double>>(std::to_string(nlines),vals));*/
      //std::cout << "vals size=" << vals.size() << std::endl;
      ++nlines;
    }
    
    void read_csv(const APIData &ad,
		  const std::string &fname)
    {
      std::ifstream csv_file(fname);
      if (!csv_file.is_open())
	throw InputConnectorBadParamException("cannot open file " + fname);
      std::string hline;
      std::getline(csv_file,hline);
      std::stringstream sg(hline);
      std::string col;
      std::string delim = ",";
      if (ad.has("separator"))
	delim = ad.get("separator").get<std::string>();
      
      // read header
      int i = 0;
      auto hit = _ignored_columns.begin();
      while(std::getline(sg,col,delim[0]))
	{
	  if ((hit=_ignored_columns.find(col))!=_ignored_columns.end())
	    continue;
	  _columns.push_back(col);
	  if (col == _label)
	    _label_pos = i;
	  ++i;
	}
      if (_label_pos < 0)
	throw InputConnectorBadParamException("cannot find label column " + _label); //TODO: only for training...
      
      //debug
      std::cout << "label=" << _label << " / pos=" << _label_pos << std::endl;
      std::cout << "CSV columns:\n";
      std::copy(_columns.begin(),_columns.end(),
		std::ostream_iterator<std::string>(std::cout," "));
      std::cout << std::endl;
      //debug

      // scaling to [0,1]
      int nlines = 0;
      bool scale = false;
      std::vector<double> min_vals, max_vals;
      if (ad.has("scale") && ad.get("scale").get<bool>())
	{
	  scale = true;
	  while(std::getline(csv_file,hline))
	    {
	      std::vector<double> vals;
	      std::string cid;
	      read_csv_line(hline,delim,vals,cid,nlines);
	      if (nlines == 1)
		min_vals = max_vals = vals;
	      else
		{
		  for (size_t j=0;j<vals.size();j++)
		    {
		      min_vals.at(j) = std::min(vals.at(j),min_vals.at(j));
		      max_vals.at(j) = std::max(vals.at(j),max_vals.at(j));
		    }
		}
	    }
	  
	  //debug
	  std::cout << "min/max scales:\n";
	  std::copy(min_vals.begin(),min_vals.end(),std::ostream_iterator<double>(std::cout," "));
	  std::cout << std::endl;
	  std::copy(max_vals.begin(),max_vals.end(),std::ostream_iterator<double>(std::cout," "));
	  std::cout << std::endl;
	  //debug
	  
	  csv_file.clear();
	  csv_file.seekg(0,std::ios::beg);
	  std::getline(csv_file,hline); // skip header line
	  nlines = 0;
	}

      // read data
      while(std::getline(csv_file,hline))
	{
	  std::vector<double> vals;
	  std::string cid;
	  read_csv_line(hline,delim,vals,cid,nlines);
	  if (scale)
	    {
	      for (size_t j=0;j<vals.size();j++)
		{
		  if (_columns.at(j) != _id && j != _label_pos && max_vals.at(j) != min_vals.at(j))
		    vals.at(j) = (vals.at(j) - min_vals.at(j)) / (max_vals.at(j) - min_vals.at(j));
		}
	    }
	  if (!_id.empty())
	    _csvdata.insert(std::pair<std::string,std::vector<double>>(cid,vals));
	  else _csvdata.insert(std::pair<std::string,std::vector<double>>(std::to_string(nlines),vals));
	  
	  //debug
	  /*std::cout << "csv data line #" << nlines << "=";
	  std::copy(vals.begin(),vals.end(),std::ostream_iterator<double>(std::cout," "));
	  std::cout << std::endl;*/
	  //debug
	}
      std::cout << "read " << nlines << " lines from " << _csv_fname << std::endl;
      //if (_id.empty())
      //_id = _columns.at(0); // default to first column
      csv_file.close();
      
      // test file, if any.
      if (!_csv_test_fname.empty())
	{
	  nlines = 0;
	  std::ifstream csv_test_file(_csv_test_fname);
	  if (!csv_test_file.is_open())
	    throw InputConnectorBadParamException("cannot open test file " + fname);
	  std::getline(csv_test_file,hline); // skip header line
	  while(std::getline(csv_test_file,hline))
	    {
	      std::vector<double> vals;
	      std::string cid;
	      read_csv_line(hline,delim,vals,cid,nlines);
	      if (scale)
		{
		  for (size_t j=0;j<vals.size();j++)
		    {
		      if (_columns.at(j) != _id && j != _label_pos && max_vals.at(j) != min_vals.at(j))
			vals.at(j) = (vals.at(j) - min_vals.at(j)) / (max_vals.at(j) - min_vals.at(j));
		    }
		}
	      if (!_id.empty())
		_csvdata_test.insert(std::pair<std::string,std::vector<double>>(cid,vals));
	      else _csvdata_test.insert(std::pair<std::string,std::vector<double>>(std::to_string(nlines),vals));
	      
	      //debug
	      /*std::cout << "csv test data line=";
	      std::copy(vals.begin(),vals.end(),std::ostream_iterator<double>(std::cout," "));
	      std::cout << std::endl;*/
	      //debug
	    }
	  std::cout << "read " << nlines << " lines from " << _csv_test_fname << std::endl;
	  csv_test_file.close();
	}
    }

    int size() const
    {
      if (!_id.empty())
	return _columns.size() - 2; // minus label and id
      else return _columns.size() - 1; // minus label
    }

    std::string _csv_fname;
    std::string _csv_test_fname;
    std::vector<std::string> _columns; //TODO: unordered map to int as pos of the column
    std::string _label;
    int _label_pos = -1;
    std::unordered_set<std::string> _ignored_columns;
    std::string _id;
    std::unordered_map<std::string,std::vector<double>> _csvdata;
    std::unordered_map<std::string,std::vector<double>> _csvdata_test;
  };
}

#endif
