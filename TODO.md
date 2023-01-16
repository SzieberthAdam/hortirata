Short term
==========
1. Ensure non-win condition when draw
1. More levels / longer campaign
1. Help quickscreen
1. Title + Logo
1. Menu
1. Tutorial
1. Better end screen

Mid term
========
1. Animal equilibrium (animals automatically take animal squares based on plant dominance)
    * Prizes: 
    * Grazing  4-3-0-0-0
    * Insects  1-3-2-4-0
    * Domestic 0-1-4-3-2
    * Game     2-3-0-4-1
    * Birds    1-2-0-3-4


    * Grazing: 2*grass0 + barn/shed + farm-stead + house
    * Insects: 2*grain1 + lettuce2 + seed4 + water + greenhouse + house + farm-stead
    * Domestic: 2*lettuce2 + house + greenhouse + farm-stead
    * Game: 2*berry3 + forest
    * Birds: 2*seed4 + berry3 + fruit tree + bushes + water
1. Sound

* Farm-stead function thinking (currently thinking on it to transform grain to grass and grass to grain)
* Mill (grain to seed and seed to grain)
* Greenhouse (seed to lettuce and lettuce to seed)
* Figure out whether I want shore tiles
* Figure out whether apply a tree/forest balancing layer; Forest icons
* City tile
* Rock / Mountain tile

Long term
=========
* Puzzle / campaign editor
* Do not mess up Windows taskbar once back from fake fullscreen. Ask Windows to refresh.
* https://raylibtech.itch.io/rrespacker


Ideas
=====

* Shore tile that requires 1 adjacent of all 5 crops
* Tile that requires 2-2 adjacent of each of 4 specific crops, hence, 5 versions
    1. No grass: Garden
    2. No wheat: City
    3. No lettuce: Harvester
    4. No berry: Farm-stead
    5. No seed:
* Live restrictions
    * Make berry live only next to fruit trees/bushes
    * Make lettuce live only next to houses/greenhouses/farm-stead
    * Fallback to pick tile if that can live there, finally to grass
* More crops
    * grapes: 6 pascal triangle
    * but what for 5?
* Turn field from cultivated to non-cultivated (cultivated should not go over 50%)
    * Natural succession:
        * Grass to bushes to fruit tree to forest (if at least 3 adjacent bushes/fruit trees)
        * Grazing animals, farm-stead, house, greenhouse all prevent grass to bushes
    * Player action:
        * Forest to bushes to ploughland
        * Build farm-stead, house, barn, shed
* Resources
    * Money:
        * grain1 harvest +1
        * lettuce2 harvest +2
        * berry3 harvest +3
        * seed4 sow -1
        * Living cost: 1 per turn
        * Bankruptcy is game over
        * Building costs
    * Wood (from forest) keep wood rate above 25% of the land.


Discarded ideas
===============

* Irrigation
    * Fields next to water are irrigated automatically
    * You can irrigate 3x3 areas adjacent to waters and wheels
    * 3 levels of soil moisture
    * Lettuce can not live on dry soil    
