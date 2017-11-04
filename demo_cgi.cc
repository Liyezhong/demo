//#include <stdio.h>
#include <stdlib.h>
#include <fcgi_stdio.h>

#include <iostream>
#include <vector>
#include <map>

using namespace std;

extern char **environ;
vector<string> split(string str)
{
    vector<string> strvec;

    string::size_type pos1, pos2;
    pos2 = str.find('&');
    pos1 = 0;
    while (string::npos != pos2) {
        strvec.push_back(str.substr(pos1, pos2 - pos1));

        pos1 = pos2 + 1;
        pos2 = str.find('&', pos1);
    }
    strvec.push_back(str.substr(pos1));
    return strvec;
}

map<string, string> keyvalue(string str)
{
    vector<string> v = split(str);

    map<string , string> m;
    string key, value;

    for (size_t i = 0; i < v.size(); i++) {
        string::size_type pos = v[i].find('=');
        key = v[i].substr(0, pos);
        value = v[i].substr(pos + 1);

        m[key] = value;
    }
    return m;
}


int main(void)
{

 char **env = environ;

 int count = 0;

 while (FCGI_Accept() >= 0) {

#if 1
    printf("Content-type: text/html\r\n"
        "\r\n"
        "<title>demo</title>"

        );
    printf("<body>");
    printf("<h3>");
#endif

    string method(getenv("REQUEST_METHOD"));
    if (method == "GET") {

    } else if (method == "POST") {
        int len = atoi(getenv("CONTENT_LENGTH"));
        printf("len: %d<br>", len);
        printf("config: <br>");
        char buf[7000];
        buf[len] = '\0';
        int ret = 0;
        char *p = buf;
            while ((p = FCGI_fgets(buf, len, stdin))){
                printf("%s", p);
            }


    }

#if 0

  std::string s(getenv("QUERY_STRING"));

  map<string, string> m = keyvalue(s);
  map<string, string>::iterator it;
  for (it = m.begin(); it != m.end(); it++)
    printf("%s --> %s  <br>", it->first.c_str(), it->second.c_str());

    printf("</h3>");
#endif

#if 0
    while (*env != NULL) {
            printf("<p>%s</p><br>", *env);
            env++;
    }
#endif

    printf("<p>count: %d</p><br>", ++count);
    printf("</body>\n");

   }
    return 0;
}
