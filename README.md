# Ironsrc (We're looking for contributors!)

> An FPS game engine that encourages users to build their own open-source FPS games.

![Built with C](https://img.shields.io/badge/language-C-blue) ![Raylib](https://img.shields.io/badge/graphics-Raylib-orange) ![Makefile](https://img.shields.io/badge/build-Makefile-lightgrey)

---

## Features (not yet all inplemented)

- Multiplayer
- Multithreaded server
- BSP map loading
- Menu system
- Modular asset system
- Entity system
- Weapons with viewmodels

---

## To do

- [ ] Handle `gm_connecting`
- [ ] Implement `serverupdate`
- [x] Implement `clientupdate`
- [ ] Menu interface using `rlgui` - up next
- [x] Add a `NetEntity` type — server only sends what client needs to render
- [ ] TAB menu
- [ ] More entity types (particles, sounds, etc.)
- [x] Chat in server struct
- [ ] Be able to chat
- [ ] Start map design — BSP or custom format
- [ ] Map triggers
- [ ] Model loading

---

## Multiplayer philosophy

`server.c` runs on a port at a fixed Hz tick rate.

1. Server accepts connection from client
2. Client sends `UserJoin`
3. Server accepts `UserJoin` (username)
4. Server replies with `UserJoinACK`
5. Client receives `UserJoinACK` (contains map and player list)

**Loop:**

6. Client listens for `ServerUpdate`
7. Server sends `ServerUpdate` (contains entities and players)
8. Client reads `ServerUpdate` and applies it to local world
   > Client removes itself from the server's entity list to avoid duplication
9. Client sends `UserUpdate` (position and velocity)
10. Server receives `UserUpdate`, validates it, and applies if reasonable → go to 6
