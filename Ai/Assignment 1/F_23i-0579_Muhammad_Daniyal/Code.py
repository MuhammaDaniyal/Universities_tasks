from collections import deque

def parse_input(filename="input.txt"):
    with open(filename, 'r') as f:
        lines = f.read().splitlines()
    
    idx = 0
    N, M = map(int, lines[idx].split())
    idx += 1
    
    grid = []
    for i in range(N):
        grid.append(lines[idx])
        idx += 1
    
    R = int(lines[idx]); idx += 1
    
    robots = []
    for _ in range(R):
        robot_id, priority, energy = map(int, lines[idx].split()); idx += 1
        startX, startY = map(int, lines[idx].split());             idx += 1
        goalX, goalY   = map(int, lines[idx].split());             idx += 1
        K = int(lines[idx]);                                       idx += 1

        checkpoints = []

        for _ in range(K):
            cx, cy = map(int, lines[idx].split());                 idx += 1
            checkpoints.append((cx, cy))

        robots.append({
            'id': robot_id,
            'priority': priority,
            'energy': energy,
            'start': (startX, startY),
            'goal': (goalX, goalY),
            'checkpoints': checkpoints
        })
    
    return N, M, grid, robots


def get_cell(grid, N, x, y):
    row = N - 1 - y
    return grid[row][x]


def is_valid(x, y, N, M, grid):
    if x < 0 or x >= M or y < 0 or y >= N:
        return False
    if get_cell(grid, N, x, y) == 'X':
        return False
    return True


def get_successors(x, y, N, M, grid):
    cell = get_cell(grid, N, x, y)
    
    all_moves = [
        (x,   y,   'wait'),
        (x,   y+1, 'up'),
        (x,   y-1, 'down'),
        (x-1, y,   'left'),
        (x+1, y,   'right'),
    ]
    
    one_way_map = {
        '^': (x,   y+1),
        'v': (x,   y-1),
        '<': (x-1, y  ),
        '>': (x+1, y  ),
    }
    
    if cell in one_way_map:
        forced_nx, forced_ny = one_way_map[cell]
        all_moves = [(forced_nx, forced_ny, 'forced')]
    
    valid_moves = []
    for nx, ny, _ in all_moves:
        if is_valid(nx, ny, N, M, grid):
            valid_moves.append((nx, ny))
    
    return valid_moves


def bfs(robot, N, M, grid, reservations=set()):
    start      = robot['start']
    goal       = robot['goal']
    checkpoints= robot['checkpoints']
    energy_limit = robot['energy']
    K = len(checkpoints)
    
    start_state = (start[0], start[1], 0, 0)  # (x, y, cp_idx, t=0)
    
    queue = deque()
    queue.append(start_state)
    
    visited = set()
    visited.add(start_state)
    
    parent = {start_state: None}
    
    while queue:
        x, y, cp_idx, t = queue.popleft()
        print(f"Visiting: (x={x}, y={y}, cp_idx={cp_idx}, t={t})")
        
        if (x, y) == goal and cp_idx == K:
            return reconstruct_path(parent, (x, y, cp_idx, t))
        
        if t >= energy_limit:
            continue
        
        for nx, ny in get_successors(x, y, N, M, grid):
            new_t = t + 1
            
            if (nx, ny, new_t) in reservations:
                continue
            
            new_cp_idx = cp_idx
            if cp_idx < K and (nx, ny) == checkpoints[cp_idx]:
                new_cp_idx = cp_idx + 1
            
            new_state = (nx, ny, new_cp_idx, new_t)
            
            if new_state not in visited:
                visited.add(new_state)
                parent[new_state] = (x, y, cp_idx, t)
                queue.append(new_state)
    
    return None


def reconstruct_path(parent, goal_state):
    path = []
    state = goal_state
    
    while state is not None:
        x, y, cp_idx, t = state
        path.append((x, y))
        state = parent[state]
    
    path.reverse()
    return path


def write_output(results, filename="output.txt"):
    results.sort(key=lambda r: r['id'])
    
    with open(filename, 'w') as f:
        for i, result in enumerate(results):
            f.write(f"Robot {result['id']}:\n")
            
            if result['path'] is None:
                f.write(f"Error: No valid path found for Robot {result['id']}\n")
            else:
                path = result['path']
                path_str = " -> ".join(f"({x},{y})" for x, y in path)
                T = len(path) - 1
                f.write(f"Path: {path_str}\n")
                f.write(f"Total Time: {T}\n")
                f.write(f"Total Energy: {T}\n")
            
            if i < len(results) - 1:
                f.write("\n")


def main():
    N, M, grid, robots = parse_input("input.txt")
    
    planning_order = sorted(robots, key=lambda r: r['priority'], reverse=True)
    
    reservations = set()
    results = []
    
    for robot in planning_order:
        path = bfs(robot, N, M, grid, reservations)
        
        if path is not None:
            for t, (x, y) in enumerate(path):
                reservations.add((x, y, t))

            gx, gy = path[-1]
            last_t = len(path) - 1
            for extra_t in range(last_t + 1, last_t + 1 + 200):
                reservations.add((gx, gy, extra_t))
        
        results.append({'id': robot['id'], 'path': path})
    
    write_output(results, "output.txt")
    print(results)


if __name__ == "__main__":
    main()