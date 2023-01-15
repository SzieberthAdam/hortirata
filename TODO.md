TODO
====
1. Farm-stead icon
1. Forest icon
1. Ensure non-win condition when draw
1. https://raylibtech.itch.io/rrespacker
1. Do not mess up Windows taskbar once back from fullscreen. Ask Windows to refresh.

Release version features
========================

1. Help quickscreen
1. Dummy tiles
    * Shore
    * City
    * Rock
    * Forest
1. Timer
1. More levels / longer campaign
1. Better end screen
1. Title / Menu
    * Logo
1. Tutorial
1. Special tiles to:
    * Replace two adjacent tiles
    * Peek outcome
    * Rotate one backward
    * Equilibrium steps (along with Remove hints)
1. Remove hints
1. Sound
1. Load any file regardless of file size
1. Puzzle / campaign editor


Ideas
=====

* Live restrictions
    * Make berry live only next to fruit trees/bushes
    * Make lettuce live only next to houses/greenhouses/farm-stead
    * Fallback to pick tile if that can live there, finally to grass
* Animal equilibrium
    * Animals automatically take animal squares based on plant dominance
    * Grazing: 2*grass0 + barn/shed + farm-stead + house
    * Insects: 2*grain1 + lettuce2 + seed4 + water + greenhouse + house + farm-stead
    * Domestic: 2*lettuce2 + house + greenhouse + farm-stead
    * Game: 2*berry3 + forest
    * Birds: 2*seed4 + berry3 + fruit tree + bushes + water
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
    * ...


Discarded ideas
===============

* Irrigation
    * Fields next to water are irrigated automatically
    * You can irrigate 3x3 areas adjacent to waters and wheels
    * 3 levels of soil moisture
    * Lettuce can not live on dry soil    
