/*
// geojson example:
{
    "type": "FeatureCollection",
    "crs": { "type": "name", "properties": { "name": "urn:ogc:def:crs:EPSG::3857" }}, // webM
    "crs": { "type": "name", "properties": { "name": "urn:ogc:def:crs:EPSG::4326" }}, // wgs84
    "features": [
        {
            "type": "Feature",
            "properties": {
            "population": 200
            },
            "geometry": {
            "type": "Point",
            "coordinates": [-112.0372, 46.608058]
            }
        }
    ]
}
*/

#include "nlohmann/json.hpp"
#include <fstream>

using json = nlohmann::json;

int main()
{
    json ft1;

    ft1["type"] = "Feature";
    ft1["properties"]["id"] = "00000001";
    ft1["geometry"]["type"] = "Point";
    ft1["geometry"]["coordinates"] = {13538498,3632079};

    json ft2;
    ft2["type"] = "Feature";
    ft2["properties"]["id"] = "00000002";
    ft2["geometry"]["type"] = "LineString";
    ft2["geometry"]["coordinates"] = {{13538419,3632080},{13537732,3637187}};

    json ft3;
    ft3["type"] = "Feature";
    ft3["properties"]["id"] = "00000003";
    ft3["geometry"]["type"] = "Polygon";
    ft3["geometry"]["coordinates"] = {{{13538419,3632080},{13541508,3632086},
                                       {13541321,3637208},{13537732,3637187},{13538419,3632080}}};

    json j;
    j["type"] = "FeatureCollection";
    j["crs"]["type"] = "name";
    j["crs"]["properties"]["name"] = "urn:ogc:def:crs:EPSG::3857";

    ////  only one element
    //    j["features"] = json::array();
    //    j["features"].push_back(ft3);
    j["features"] = {ft1, ft2, ft3};
    std::ofstream o("example.geojson");
    o << j.dump(4);

    return 0;
}